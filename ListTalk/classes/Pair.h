/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Pair__
#define H__ListTalk__Pair__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/classes/List.h>
#include <ListTalk/vm/value.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

typedef struct LT_Pair_s LT_Pair;
extern LT_Class LT_Pair_class;
extern LT_Class LT_Pair_class_class;

extern LT_Class LT_MutablePair_class;
extern LT_Class LT_MutablePair_class_class;

static inline int LT_Pair_p(LT_Value value){
    return LT_VALUE_IS_POINTER(value)
        && (LT_VALUE_POINTER_TAG(value) == LT_VALUE_POINTER_TAG_PAIR
            || LT_VALUE_POINTER_TAG(value) == LT_VALUE_POINTER_TAG_IMMUTABLE_LIST);
}

static inline int LT_MutablePair_p(LT_Value value){
    return LT_VALUE_IS_POINTER(value)
        && LT_VALUE_POINTER_TAG(value) == LT_VALUE_POINTER_TAG_PAIR;
}

static inline LT_MutablePair* LT_MutablePair_from_value(LT_Value value){
    if (!LT_MutablePair_p(value)){
        LT_type_error(value, &LT_MutablePair_class);
    }
    return (LT_MutablePair*)LT_VALUE_POINTER_VALUE(value);
}

LT_Value LT_cons(LT_Value car, LT_Value cdr);
LT_Value LT_list(LT_Value first, ...);
LT_Value LT_list_with_rest(LT_Value first, ...);
void LT_Pair_set_car(LT_Value pair, LT_Value value);
void LT_Pair_set_cdr(LT_Value pair, LT_Value value);

LT__END_DECLS

#endif
