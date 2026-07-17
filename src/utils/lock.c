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

void LT_Mutex_lock_slow(LT_Mutex* mutex)
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

        while (atomic_load_explicit(mutex, memory_order_relaxed) !=
               LT_MUTEX_UNLOCKED){
            pthread_cond_wait(&queue->cond, &queue->mutex);
        }
    }
}

void LT_Mutex_unlock_slow(LT_Mutex* mutex)
{
    LT_LockWaitQueue* queue = LT_lock_wait_queue_for(mutex);

    pthread_mutex_lock(&queue->mutex);
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
}

void LT_RWLock_read_lock_slow(LT_RWLock* lock)
{
    LT_LockWaitQueue* queue = LT_lock_wait_queue_for(lock);

    pthread_mutex_lock(&queue->mutex);

    for (;;){
        uintptr_t state = atomic_load_explicit(lock, memory_order_relaxed);

        while ((state & (LT_RWLOCK_WRITER | LT_RWLOCK_WAITERS)) == 0){
            if (atomic_compare_exchange_weak_explicit(
                    lock,
                    &state,
                    state + LT_RWLOCK_READER,
                    memory_order_acquire,
                    memory_order_relaxed
                )){
                pthread_mutex_unlock(&queue->mutex);
                return;
            }
        }

        if ((state & LT_RWLOCK_WAITERS) == 0){
            uintptr_t expected = state;
            if (!atomic_compare_exchange_weak_explicit(
                    lock,
                    &expected,
                    state | LT_RWLOCK_WAITERS,
                    memory_order_relaxed,
                    memory_order_relaxed
                )){
                continue;
            }
        }

        while (atomic_load_explicit(lock, memory_order_relaxed) &
               (LT_RWLOCK_WRITER | LT_RWLOCK_WAITERS)){
            pthread_cond_wait(&queue->cond, &queue->mutex);
        }
    }
}

void LT_RWLock_write_lock_slow(LT_RWLock* lock)
{
    LT_LockWaitQueue* queue = LT_lock_wait_queue_for(lock);

    pthread_mutex_lock(&queue->mutex);

    for (;;){
        uintptr_t state = atomic_load_explicit(lock, memory_order_relaxed);

        while ((state & ~((uintptr_t) LT_RWLOCK_WAITERS)) == 0){
            uintptr_t desired = LT_RWLOCK_WRITER | (state & LT_RWLOCK_WAITERS);

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

        atomic_fetch_or_explicit(lock, LT_RWLOCK_WAITERS, memory_order_relaxed);

        while (atomic_load_explicit(lock, memory_order_relaxed) &
               ~((uintptr_t) LT_RWLOCK_WAITERS)){
            pthread_cond_wait(&queue->cond, &queue->mutex);
        }
    }
}

void LT_RWLock_unlock_slow(LT_RWLock* lock)
{
    LT_LockWaitQueue* queue = LT_lock_wait_queue_for(lock);

    pthread_mutex_lock(&queue->mutex);
    atomic_fetch_and_explicit(
        lock,
        ~((uintptr_t) LT_RWLOCK_WAITERS),
        memory_order_relaxed
    );
    pthread_cond_broadcast(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
}
