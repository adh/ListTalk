/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Dictionary__
#define H__ListTalk__Dictionary__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

typedef struct LT_Dictionary_s LT_Dictionary;
typedef LT_Dictionary LT_ImmutableDictionary;

extern LT_Class LT_ImmutableDictionary_class;
extern LT_Class LT_ImmutableDictionary_class_class;
extern LT_Class LT_Dictionary_class;
extern LT_Class LT_Dictionary_class_class;

static inline int LT_ImmutableDictionary_p(LT_Value value){
    return LT_Value_is_instance_of(
        value,
        (LT_Value)(uintptr_t)&LT_ImmutableDictionary_class
    );
}

static inline int LT_Dictionary_p(LT_Value value){
    return LT_ImmutableDictionary_p(value);
}

static inline LT_ImmutableDictionary* LT_ImmutableDictionary_from_value(
    LT_Value value
){
    if (!LT_ImmutableDictionary_p(value)){
        LT_type_error(value, &LT_ImmutableDictionary_class);
    }
    return (LT_ImmutableDictionary*)LT_VALUE_POINTER_VALUE(value);
}

static inline LT_Dictionary* LT_Dictionary_from_value(LT_Value value){
    if (!LT_Dictionary_p(value)){
        LT_type_error(value, &LT_Dictionary_class);
    }
    return (LT_Dictionary*)LT_VALUE_POINTER_VALUE(value);
}

LT_ImmutableDictionary* LT_ImmutableDictionary_new(void);
LT_ImmutableDictionary* LT_ImmutableDictionary_newFromAList(LT_Value alist);
LT_Dictionary* LT_Dictionary_new(void);
LT_Dictionary* LT_Dictionary_newFromAList(LT_Value alist);
size_t LT_Dictionary_size(LT_Dictionary* dictionary);
LT_Value LT_Dictionary_asAList(LT_Dictionary* dictionary);
void LT_Dictionary_for_each(LT_Dictionary* dictionary, LT_Value callable);
LT_Value LT_Dictionary_map(LT_Dictionary* dictionary, LT_Value callable);
void LT_Dictionary_atPut(
    LT_Dictionary* dictionary,
    LT_Value key,
    LT_Value value
);
int LT_Dictionary_at(
    LT_Dictionary* dictionary,
    LT_Value key,
    LT_Value* value_out
);
int LT_Dictionary_remove(
    LT_Dictionary* dictionary,
    LT_Value key,
    LT_Value* value_out
);

LT__END_DECLS

#endif
