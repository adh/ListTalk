/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Float__
#define H__ListTalk__Float__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/vm/error.h>

#include <string.h>

LT__BEGIN_DECLS

typedef struct LT_Float_s LT_Float;

extern LT_Class LT_Float_class;
extern LT_Class LT_Float_class_class;

static inline int LT_Float_p(LT_Value value){
    return LT_VALUE_IS_IMMEDIATE(value)
        && LT_VALUE_IMMEDIATE_TAG_IS_FLONUM(LT_VALUE_IMMEDIATE_TAG(value));
}

static inline LT_Value LT_Float_new(double number){
    uint64_t raw;
    uint64_t packed;
    uint64_t remainder;

    memcpy(&raw, &number, sizeof(raw));

    packed = raw >> 2;
    remainder = raw & UINT64_C(0x3);

    /* Round-to-nearest-even while removing two least-significant bits. */
    if (remainder > 2 || (remainder == 2 && (packed & UINT64_C(1)) != 0)){
        if (packed != UINT64_C(0x3fffffffffffffff)){
            packed++;
        }
    }

    return (LT_Value)(UINT64_C(0xc000000000000000) | packed);
}

static inline double LT_Float_value(LT_Value value){
    uint64_t raw;
    double number;

    if (!LT_Float_p(value)){
        LT_type_error(value, &LT_Float_class);
    }

    raw = (((uint64_t)value)
        & UINT64_C(0x3fffffffffffffff)) << 2;

    memcpy(&number, &raw, sizeof(number));
    return number;
}

LT__END_DECLS

#endif
