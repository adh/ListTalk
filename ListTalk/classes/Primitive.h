/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Primitive__
#define H__ListTalk__Primitive__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Primitive);

typedef LT_Value(*LT_Primitive_Func)(LT_Value arguments);

LT_Value LT_Primitive_new(char* name, LT_Primitive_Func function);
char* LT_Primitive_name(LT_Primitive* primitive);
LT_Primitive_Func LT_Primitive_function(LT_Primitive* primitive);
LT_Value LT_Primitive_call(LT_Value primitive, LT_Value arguments);

static inline int LT_Value_is_primitive(LT_Value value){
    return LT_VALUE_IS_POINTER(value)
        && LT_VALUE_POINTER_TAG(value) == LT_VALUE_POINTER_TAG_PRIMITIVE;
}

LT__END_DECLS

#endif
