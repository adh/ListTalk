/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__MethodDescriptor__
#define H__ListTalk__MethodDescriptor__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_MethodDescriptor);

LT_Value LT_MethodDescriptor_new(
    LT_Value selector,
    LT_Value callable,
    LT_Value implementing_class
);
LT_Value LT_MethodDescriptor_selector(LT_MethodDescriptor* method);
LT_Value LT_MethodDescriptor_callable(LT_MethodDescriptor* method);
LT_Value LT_MethodDescriptor_implementing_class(LT_MethodDescriptor* method);

LT__END_DECLS

#endif
