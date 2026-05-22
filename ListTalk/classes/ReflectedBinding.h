/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__ReflectedBinding__
#define H__ListTalk__ReflectedBinding__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_ReflectedBinding);

LT_Value LT_ReflectedBinding_new(
    LT_Value symbol,
    LT_Value value,
    unsigned int flags
);
LT_Value LT_ReflectedBinding_symbol(LT_ReflectedBinding* binding);
LT_Value LT_ReflectedBinding_value(LT_ReflectedBinding* binding);
unsigned int LT_ReflectedBinding_flags(LT_ReflectedBinding* binding);

LT__END_DECLS

#endif
