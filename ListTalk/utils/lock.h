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

void LT_Mutex_init(LT_Mutex* mutex);
void LT_Mutex_lock(LT_Mutex* mutex);
int LT_Mutex_try_lock(LT_Mutex* mutex);
void LT_Mutex_unlock(LT_Mutex* mutex);

void LT_RWLock_init(LT_RWLock* lock);
void LT_RWLock_read_lock(LT_RWLock* lock);
int LT_RWLock_try_read_lock(LT_RWLock* lock);
void LT_RWLock_read_unlock(LT_RWLock* lock);
void LT_RWLock_write_lock(LT_RWLock* lock);
int LT_RWLock_try_write_lock(LT_RWLock* lock);
void LT_RWLock_write_unlock(LT_RWLock* lock);

LT__END_DECLS

#endif
