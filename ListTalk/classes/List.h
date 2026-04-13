/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__List__
#define H__ListTalk__List__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/classes/ImmutableList.h>
#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

#include <stdio.h>

LT__BEGIN_DECLS

typedef struct LT_List_s LT_List;
extern LT_Class LT_List_class;
extern LT_Class LT_List_class_class;

typedef struct LT_MutablePair_s {
    LT_Value car;
    LT_Value cdr;
} LT_MutablePair;

size_t LT_List_hash(LT_Value value);
int LT_List_equal_p(LT_Value left, LT_Value right);
void LT_List_debugPrintOn(LT_Value value, FILE* stream);
LT_Value LT_List_map(LT_Value callable, LT_Value list);
LT_Value LT_List_map_many(
    LT_Value callable,
    size_t list_count,
    const LT_Value* lists
);

static inline int LT_List_p(LT_Value value){
    return value == LT_NIL
        || (LT_VALUE_IS_POINTER(value)
            && (LT_VALUE_POINTER_TAG(value) == LT_VALUE_POINTER_TAG_PAIR
                || LT_VALUE_POINTER_TAG(value) == LT_VALUE_POINTER_TAG_IMMUTABLE_LIST));
}

static inline LT_Value LT_car(LT_Value pair){
    if (LT_VALUE_POINTER_TAG(pair) == LT_VALUE_POINTER_TAG_IMMUTABLE_LIST){
        return LT_ImmutableList_car(pair);
    }
    return ((LT_MutablePair*)LT_VALUE_POINTER_VALUE(pair))->car;
}

static inline LT_Value LT_cdr(LT_Value pair){
    if (LT_VALUE_POINTER_TAG(pair) == LT_VALUE_POINTER_TAG_IMMUTABLE_LIST){
        return LT_ImmutableList_cdr(pair);
    }
    return ((LT_MutablePair*)LT_VALUE_POINTER_VALUE(pair))->cdr;
}

LT__END_DECLS

#endif
