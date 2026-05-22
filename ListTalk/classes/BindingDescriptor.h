/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__BindingDescriptor__
#define H__ListTalk__BindingDescriptor__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_BindingDescriptor);

LT_Value LT_BindingDescriptor_new(
    LT_Value symbol,
    LT_Value value,
    unsigned int flags
);
LT_Value LT_BindingDescriptor_symbol(LT_BindingDescriptor* binding);
LT_Value LT_BindingDescriptor_value(LT_BindingDescriptor* binding);
unsigned int LT_BindingDescriptor_flags(LT_BindingDescriptor* binding);

LT__END_DECLS

#endif
