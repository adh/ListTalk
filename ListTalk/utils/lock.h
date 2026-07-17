/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__utils__lock__
#define H__ListTalk__utils__lock__

#include <ListTalk/macros/env_macros.h>

#include <stdatomic.h>

LT__BEGIN_DECLS

typedef _Atomic(uintptr_t) LT_Mutex;
typedef _Atomic(uintptr_t) LT_RWLock;

#define LT_MUTEX_INITIALIZER ATOMIC_VAR_INIT(0)
#define LT_RWLOCK_INITIALIZER ATOMIC_VAR_INIT(0)

enum {
    LT_MUTEX_UNLOCKED = 0,
    LT_MUTEX_LOCKED = 1,
    LT_MUTEX_LOCKED_CONTENDED = 2,
};

enum {
    LT_RWLOCK_WRITER = 1,
    LT_RWLOCK_WAITERS = 2,
    LT_RWLOCK_READER = 4,
};

void LT_Mutex_lock_slow(LT_Mutex* mutex);
void LT_Mutex_unlock_slow(LT_Mutex* mutex);

void LT_RWLock_read_lock_slow(LT_RWLock* lock);
void LT_RWLock_write_lock_slow(LT_RWLock* lock);
void LT_RWLock_unlock_slow(LT_RWLock* lock);

static inline void LT_Mutex_init(LT_Mutex* mutex)
{
    atomic_init(mutex, LT_MUTEX_UNLOCKED);
}

static inline int LT_Mutex_try_lock(LT_Mutex* mutex)
{
    uintptr_t expected = LT_MUTEX_UNLOCKED;
    return atomic_compare_exchange_strong_explicit(
        mutex,
        &expected,
        LT_MUTEX_LOCKED,
        memory_order_acquire,
        memory_order_relaxed
    );
}

static inline void LT_Mutex_lock(LT_Mutex* mutex)
{
    if (LT_Mutex_try_lock(mutex)){
        return;
    }
    LT_Mutex_lock_slow(mutex);
}

static inline void LT_Mutex_unlock(LT_Mutex* mutex)
{
    uintptr_t old_state = atomic_exchange_explicit(
        mutex,
        LT_MUTEX_UNLOCKED,
        memory_order_release
    );
    if (old_state == LT_MUTEX_LOCKED_CONTENDED){
        LT_Mutex_unlock_slow(mutex);
    }
}

static inline void LT_RWLock_init(LT_RWLock* lock)
{
    atomic_init(lock, 0);
}

static inline int LT_RWLock_try_read_lock(LT_RWLock* lock)
{
    uintptr_t state = atomic_load_explicit(lock, memory_order_relaxed);

    while ((state & (LT_RWLOCK_WRITER | LT_RWLOCK_WAITERS)) == 0){
        if (atomic_compare_exchange_weak_explicit(
                lock,
                &state,
                state + LT_RWLOCK_READER,
                memory_order_acquire,
                memory_order_relaxed
            )){
            return 1;
        }
    }

    return 0;
}

static inline void LT_RWLock_read_lock(LT_RWLock* lock)
{
    if (LT_RWLock_try_read_lock(lock)){
        return;
    }
    LT_RWLock_read_lock_slow(lock);
}

static inline int LT_RWLock_try_write_lock(LT_RWLock* lock)
{
    uintptr_t expected = 0;
    return atomic_compare_exchange_strong_explicit(
        lock,
        &expected,
        LT_RWLOCK_WRITER,
        memory_order_acquire,
        memory_order_relaxed
    );
}

static inline void LT_RWLock_write_lock(LT_RWLock* lock)
{
    if (LT_RWLock_try_write_lock(lock)){
        return;
    }
    LT_RWLock_write_lock_slow(lock);
}

static inline void LT_RWLock_read_unlock(LT_RWLock* lock)
{
    uintptr_t old_state = atomic_fetch_sub_explicit(
        lock,
        LT_RWLOCK_READER,
        memory_order_release
    );
    if (old_state == (LT_RWLOCK_READER | LT_RWLOCK_WAITERS)){
        LT_RWLock_unlock_slow(lock);
    }
}

static inline void LT_RWLock_write_unlock(LT_RWLock* lock)
{
    uintptr_t old_state = atomic_exchange_explicit(
        lock,
        0,
        memory_order_release
    );
    if (old_state & LT_RWLOCK_WAITERS){
        LT_RWLock_unlock_slow(lock);
    }
}

LT__END_DECLS

#endif
