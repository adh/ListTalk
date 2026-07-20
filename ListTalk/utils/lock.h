/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__utils__lock__
#define H__ListTalk__utils__lock__

#include <ListTalk/macros/env_macros.h>

#include <limits.h>
#include <stdatomic.h>

LT__BEGIN_DECLS

typedef _Atomic(uintptr_t) LT_MutexWord;
typedef _Atomic(uintptr_t) LT_RWLockWord;

#define LT_MUTEX_INITIALIZER ATOMIC_VAR_INIT(0)
#define LT_RWLOCK_INITIALIZER ATOMIC_VAR_INIT(0)

enum {
    LT_MUTEX_UNLOCKED = 0,
    LT_MUTEX_LOCKED = 1,
    LT_MUTEX_LOCKED_CONTENDED = 2,
};

#define LT_RWLOCK_WRITER ((uintptr_t) 1)
#define LT_RWLOCK_READER ((uintptr_t) 2)
#define LT_RWLOCK_WAITERS ((uintptr_t) 1 << (sizeof(uintptr_t) * CHAR_BIT / 2))
#define LT_RWLOCK_ACTIVE_MASK (LT_RWLOCK_WAITERS - 1)

void LT_MutexWord_lock_slow(LT_MutexWord* mutex);
void LT_MutexWord_unlock_slow(LT_MutexWord* mutex);

void LT_RWLockWord_read_lock_slow(LT_RWLockWord* lock);
void LT_RWLockWord_write_lock_slow(LT_RWLockWord* lock);
void LT_RWLockWord_unlock_slow(LT_RWLockWord* lock);

static inline void LT_MutexWord_init(LT_MutexWord* mutex)
{
    atomic_init(mutex, LT_MUTEX_UNLOCKED);
}

static inline int LT_MutexWord_try_lock(LT_MutexWord* mutex)
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

static inline void LT_MutexWord_lock(LT_MutexWord* mutex)
{
    if (LT_MutexWord_try_lock(mutex)){
        return;
    }
    LT_MutexWord_lock_slow(mutex);
}

static inline void LT_MutexWord_unlock(LT_MutexWord* mutex)
{
    uintptr_t old_state = atomic_exchange_explicit(
        mutex,
        LT_MUTEX_UNLOCKED,
        memory_order_release
    );
    if (old_state == LT_MUTEX_LOCKED_CONTENDED){
        LT_MutexWord_unlock_slow(mutex);
    }
}

static inline void LT_RWLockWord_init(LT_RWLockWord* lock)
{
    atomic_init(lock, 0);
}

static inline int LT_RWLockWord_try_read_lock(LT_RWLockWord* lock)
{
    uintptr_t state = atomic_load_explicit(lock, memory_order_relaxed);

    while ((state & LT_RWLOCK_WRITER) == 0 && state < LT_RWLOCK_WAITERS){
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

static inline void LT_RWLockWord_read_lock(LT_RWLockWord* lock)
{
    if (LT_RWLockWord_try_read_lock(lock)){
        return;
    }
    LT_RWLockWord_read_lock_slow(lock);
}

static inline int LT_RWLockWord_try_write_lock(LT_RWLockWord* lock)
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

static inline void LT_RWLockWord_write_lock(LT_RWLockWord* lock)
{
    if (LT_RWLockWord_try_write_lock(lock)){
        return;
    }
    LT_RWLockWord_write_lock_slow(lock);
}

static inline void LT_RWLockWord_read_unlock(LT_RWLockWord* lock)
{
    uintptr_t old_state = atomic_fetch_sub_explicit(
        lock,
        LT_RWLOCK_READER,
        memory_order_release
    );
    uintptr_t new_state = old_state - LT_RWLOCK_READER;

    if ((new_state & LT_RWLOCK_ACTIVE_MASK) == 0 &&
        new_state >= LT_RWLOCK_WAITERS){
        LT_RWLockWord_unlock_slow(lock);
    }
}

static inline void LT_RWLockWord_write_unlock(LT_RWLockWord* lock)
{
    uintptr_t old_state = atomic_fetch_and_explicit(
        lock,
        ~LT_RWLOCK_WRITER,
        memory_order_release
    );
    uintptr_t new_state = old_state & ~LT_RWLOCK_WRITER;

    if ((new_state & LT_RWLOCK_ACTIVE_MASK) == 0 &&
        new_state >= LT_RWLOCK_WAITERS){
        LT_RWLockWord_unlock_slow(lock);
    }
}

LT__END_DECLS

#endif
