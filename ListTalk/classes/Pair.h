/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Pair__
#define H__ListTalk__Pair__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Pair);

static inline int LT_Value_is_pair(LT_Value value){
    return LT_VALUE_IS_POINTER(value)
        && LT_VALUE_POINTER_TAG(value) == LT_VALUE_POINTER_TAG_PAIR;
}

LT_Value LT_cons(LT_Value car, LT_Value cdr);
LT_Value LT_list(LT_Value first, ...);
LT_Value LT_list_with_rest(LT_Value first, ...);
LT_Value LT_car(LT_Value pair);
LT_Value LT_cdr(LT_Value pair);

void LT_Pair_set_car(LT_Value pair, LT_Value value);
void LT_Pair_set_cdr(LT_Value pair, LT_Value value);

LT__END_DECLS

#endif
