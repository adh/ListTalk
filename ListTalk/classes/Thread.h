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
LT_Value LT_Thread_join(LT_Thread* thread);
void LT_Thread_makeDaemon(LT_Thread* thread);

LT__END_DECLS

#endif
