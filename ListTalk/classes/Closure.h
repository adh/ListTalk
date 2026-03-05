/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Closure__
#define H__ListTalk__Closure__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/vm/Environment.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Closure);

LT_Value LT_Closure_new(
    LT_Value parameters,
    LT_Value body,
    LT_Environment* environment
);
LT_Value LT_Closure_parameters(LT_Closure* closure);
LT_Value LT_Closure_body(LT_Closure* closure);
LT_Environment* LT_Closure_environment(LT_Closure* closure);

static inline int LT_Value_is_closure(LT_Value value){
    return LT_VALUE_IS_POINTER(value)
        && LT_VALUE_POINTER_TAG(value) == LT_VALUE_POINTER_TAG_CLOSURE;
}

LT__END_DECLS

#endif
