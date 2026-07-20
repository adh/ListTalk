/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Mutex__
#define H__ListTalk__Mutex__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/utils/lock.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Mutex);

LT_Mutex* LT_Mutex_new(void);
void LT_Mutex_lock(LT_Mutex* mutex);
int LT_Mutex_tryLock(LT_Mutex* mutex);
void LT_Mutex_unlock(LT_Mutex* mutex);

LT__END_DECLS

#endif
