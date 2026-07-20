/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/utils/lock.h>

enum {
    LT_LOCK_WAIT_QUEUE_COUNT = 64,
};

typedef struct LT_LockWaitQueue LT_LockWaitQueue;

struct LT_LockWaitQueue {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

static LT_LockWaitQueue LT_lock_wait_queues[LT_LOCK_WAIT_QUEUE_COUNT];
static pthread_once_t LT_lock_wait_queues_once = PTHREAD_ONCE_INIT;

static void LT_lock_wait_queues_init(void)
{
    for (size_t i = 0; i < LT_LOCK_WAIT_QUEUE_COUNT; ++i){
        pthread_mutex_init(&LT_lock_wait_queues[i].mutex, NULL);
        pthread_cond_init(&LT_lock_wait_queues[i].cond, NULL);
    }
}

static LT_LockWaitQueue* LT_lock_wait_queue_for(const void* address)
{
    uintptr_t hash = (uintptr_t) address;

    pthread_once(&LT_lock_wait_queues_once, LT_lock_wait_queues_init);

    hash ^= hash >> 33;
    hash *= (uintptr_t) 0xff51afd7ed558ccdULL;
    hash ^= hash >> 33;

    return &LT_lock_wait_queues[hash & (LT_LOCK_WAIT_QUEUE_COUNT - 1)];
}

void LT_MutexWord_lock_slow(LT_MutexWord* mutex)
{
    LT_LockWaitQueue* queue = LT_lock_wait_queue_for(mutex);

    pthread_mutex_lock(&queue->mutex);

    for (;;){
        uintptr_t old_state = atomic_exchange_explicit(
            mutex,
            LT_MUTEX_LOCKED_CONTENDED,
            memory_order_acquire
        );

        if (old_state == LT_MUTEX_UNLOCKED){
            pthread_mutex_unlock(&queue->mutex);
            return;
        }

        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
}

void LT_MutexWord_unlock_slow(LT_MutexWord* mutex)
{
    LT_LockWaitQueue* queue = LT_lock_wait_queue_for(mutex);

    pthread_mutex_lock(&queue->mutex);
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
}

void LT_RWLockWord_read_lock_slow(LT_RWLockWord* lock)
{
    LT_LockWaitQueue* queue = LT_lock_wait_queue_for(lock);

    pthread_mutex_lock(&queue->mutex);
    atomic_fetch_add_explicit(lock, LT_RWLOCK_WAITERS, memory_order_relaxed);

    for (;;){
        uintptr_t state = atomic_load_explicit(lock, memory_order_relaxed);

        while ((state & LT_RWLOCK_WRITER) == 0){
            if (atomic_compare_exchange_weak_explicit(
                    lock,
                    &state,
                    (state - LT_RWLOCK_WAITERS) + LT_RWLOCK_READER,
                    memory_order_acquire,
                    memory_order_relaxed
                )){
                pthread_mutex_unlock(&queue->mutex);
                return;
            }
        }

        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
}

void LT_RWLockWord_write_lock_slow(LT_RWLockWord* lock)
{
    LT_LockWaitQueue* queue = LT_lock_wait_queue_for(lock);

    pthread_mutex_lock(&queue->mutex);
    atomic_fetch_add_explicit(lock, LT_RWLOCK_WAITERS, memory_order_relaxed);

    for (;;){
        uintptr_t state = atomic_load_explicit(lock, memory_order_relaxed);

        while ((state & LT_RWLOCK_ACTIVE_MASK) == 0){
            uintptr_t desired = (state - LT_RWLOCK_WAITERS) |
                LT_RWLOCK_WRITER;

            if (atomic_compare_exchange_weak_explicit(
                    lock,
                    &state,
                    desired,
                    memory_order_acquire,
                    memory_order_relaxed
                )){
                pthread_mutex_unlock(&queue->mutex);
                return;
            }
        }

        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
}

void LT_RWLockWord_unlock_slow(LT_RWLockWord* lock)
{
    LT_LockWaitQueue* queue = LT_lock_wait_queue_for(lock);

    pthread_mutex_lock(&queue->mutex);
    pthread_cond_broadcast(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
}
