/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__SmallInteger__
#define H__ListTalk__SmallInteger__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/vm/error.h>

LT__BEGIN_DECLS

typedef struct LT_SmallInteger_s LT_SmallInteger;

extern LT_Class LT_SmallInteger_class;
extern LT_Class LT_SmallInteger_class_class;

static inline int LT_SmallInteger_in_range(int64_t value){
    return value >= LT_VALUE_FIXNUM_MIN && value <= LT_VALUE_FIXNUM_MAX;
}

/* SmallInteger values are immediate fixnums, not heap objects. */
static inline int LT_SmallInteger_p(LT_Value value){
    return LT_Value_class(value) == &LT_SmallInteger_class;
}

static inline int64_t LT_SmallInteger_value(LT_Value value){
    uint64_t raw;

    if (!LT_SmallInteger_p(value)){
        LT_type_error(value, &LT_SmallInteger_class);
    }
    raw = LT_VALUE_IMMEDIATE_VALUE(value);

    if (raw & (UINT64_C(1) << 55)){
        raw |= UINT64_C(0xff00000000000000);
    }

    return (int64_t)raw;
}

static inline LT_Value LT_SmallInteger_new(int64_t value){
    if (!LT_SmallInteger_in_range(value)){
        LT_error("SmallInteger out of range");
    }
    return LT_VALUE_MAKE_IMMEDIATE(
        LT_VALUE_IMMEDIATE_TAG_FIXNUM,
        (uint64_t)value
    );
}

LT__END_DECLS

#endif
