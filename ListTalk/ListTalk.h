/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__ListTalk__
#define H__ListTalk__ListTalk__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/classes/Nil.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/Pair.h>

LT__BEGIN_DECLS

typedef struct LT_VMInstanceState_s LT_VMInstanceState;
typedef struct LT_VMThreadState_s LT_VMThreadState;
typedef struct LT_VMTailCallState_s LT_VMTailCallState;

extern void LT_init(void);

extern LT_Object* LT_eval(LT_Object* expression);

LT__END_DECLS

#endif
