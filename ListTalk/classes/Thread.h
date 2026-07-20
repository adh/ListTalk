/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Thread__
#define H__ListTalk__Thread__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/utils/lock.h>
#include <ListTalk/vm/value.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Thread);

LT_Thread* LT_Thread_new(LT_Value callable);
LT_Thread* LT_Thread_current(void);

LT_Value LT_Thread_join(LT_Thread* thread);
void LT_Thread_makeDetached(LT_Thread* thread);

bool LT_Thread_finished_p(LT_Thread* thread);
bool LT_Thread_joined_p(LT_Thread* thread);
bool LT_Thread_detached_p(LT_Thread* thread);
LT_Value LT_Thread_result(LT_Thread* thread);

LT__END_DECLS

#endif
