/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Macro__
#define H__ListTalk__Macro__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Macro);

LT_Value LT_Macro_new(LT_Value callable);
LT_Value LT_Macro_callable(LT_Macro* macro);

static inline int LT_Value_is_macro(LT_Value value){
    return LT_VALUE_IS_POINTER(value)
        && LT_VALUE_POINTER_TAG(value) == LT_VALUE_POINTER_TAG_MACRO;
}

LT__END_DECLS

#endif
