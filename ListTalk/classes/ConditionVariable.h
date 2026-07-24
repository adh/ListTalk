/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__ConditionVariable__
#define H__ListTalk__ConditionVariable__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/classes/Mutex.h>
#include <ListTalk/utils/lock.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_ConditionVariable);

LT_ConditionVariable* LT_ConditionVariable_new(void);
LT_ConditionVariable* LT_ConditionVariable_new_named(LT_Value name);
void LT_ConditionVariable_wait(LT_ConditionVariable* cond, LT_Mutex* mutex);
void LT_ConditionVariable_signal(LT_ConditionVariable* cond);
void LT_ConditionVariable_broadcast(LT_ConditionVariable* cond);
LT_Value LT_ConditionVariable_name(LT_ConditionVariable* cond);

LT__END_DECLS

#endif
