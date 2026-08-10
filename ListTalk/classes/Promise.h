/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Promise__
#define H__ListTalk__Promise__

#include <ListTalk/classes/Future.h>
#include <ListTalk/macros/env_macros.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/vm/value.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Promise);

LT_Promise* LT_Promise_delay(LT_Value thunk);
LT_Value LT_Promise_force(LT_Promise* promise);
LT_Value LT_Promise_value(LT_Promise* promise);
bool LT_Promise_hasValue_p(LT_Promise* promise);

LT__END_DECLS

#endif
