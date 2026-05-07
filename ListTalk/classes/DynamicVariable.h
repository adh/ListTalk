/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__DynamicVariable__
#define H__ListTalk__DynamicVariable__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/vm/value.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_DynamicVariable);

LT_DynamicVariable* LT_DynamicVariable_new(LT_Value default_value);
LT_Value LT_DynamicVariable_value(LT_DynamicVariable* variable);
void LT_DynamicVariable_setValue(LT_DynamicVariable* variable, LT_Value value);

LT__END_DECLS

#endif
