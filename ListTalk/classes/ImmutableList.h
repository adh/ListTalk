/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__ImmutableList__
#define H__ListTalk__ImmutableList__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/vm/value.h>
#include <ListTalk/vm/Class.h>

#include <stddef.h>

LT__BEGIN_DECLS

extern LT_Class LT_ImmutableList_class;
extern LT_Class LT_ImmutableList_class_class;

enum {
    LT_IMMUTABLE_LIST_FLAG_CDR_TAIL = 1u << 0,
    LT_IMMUTABLE_LIST_FLAG_SOURCE_LOCATION = 1u << 1,
    LT_IMMUTABLE_LIST_FLAG_ORIGINAL_EXPRESSION = 1u << 2,
};

static inline int LT_ImmutableList_p(LT_Value value){
    return LT_Value_class(value) == &LT_ImmutableList_class;
}

LT_Value LT_ImmutableList_new(size_t count, const LT_Value* values);
LT_Value LT_ImmutableList_new_with_tail(
    size_t count,
    const LT_Value* values,
    LT_Value tail
);
LT_Value LT_ImmutableList_car(LT_Value value);
LT_Value LT_ImmutableList_cdr(LT_Value value);

LT__END_DECLS

#endif
