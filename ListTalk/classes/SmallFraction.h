/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__SmallFraction__
#define H__ListTalk__SmallFraction__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/vm/error.h>

LT__BEGIN_DECLS

typedef struct LT_SmallFraction_s LT_SmallFraction;

extern LT_Class LT_SmallFraction_class;
extern LT_Class LT_SmallFraction_class_class;

#define LT_SMALL_FRACTION_NUMERATOR_BITS 31
#define LT_SMALL_FRACTION_DENOMINATOR_BITS 24
#define LT_SMALL_FRACTION_NUMERATOR_MAX ((INT64_C(1) << (LT_SMALL_FRACTION_NUMERATOR_BITS - 1)) - 1)
#define LT_SMALL_FRACTION_NUMERATOR_MIN (-(INT64_C(1) << (LT_SMALL_FRACTION_NUMERATOR_BITS - 1)))
#define LT_SMALL_FRACTION_DENOMINATOR_MAX ((UINT32_C(1) << LT_SMALL_FRACTION_DENOMINATOR_BITS) - 1)

static inline int LT_SmallFraction_p(LT_Value value){
    return LT_VALUE_IS_IMMEDIATE(value)
        && LT_VALUE_IMMEDIATE_TAG(value) == LT_VALUE_IMMEDIATE_TAG_SMALL_FRACTION;
}

static inline int LT_SmallFraction_in_range(int64_t numerator, uint32_t denominator){
    if (denominator <= 1 || denominator > LT_SMALL_FRACTION_DENOMINATOR_MAX){
        return 0;
    }
    if (numerator == 0){
        return 0;
    }

    return numerator >= LT_SMALL_FRACTION_NUMERATOR_MIN
        && numerator <= LT_SMALL_FRACTION_NUMERATOR_MAX;
}

static inline int64_t LT_SmallFraction_numerator(LT_Value value){
    uint64_t raw;
    uint64_t encoded;

    if (!LT_SmallFraction_p(value)){
        LT_type_error(value, &LT_SmallFraction_class);
    }

    raw = LT_VALUE_IMMEDIATE_VALUE(value);
    encoded = (raw >> LT_SMALL_FRACTION_DENOMINATOR_BITS)
        & ((UINT64_C(1) << LT_SMALL_FRACTION_NUMERATOR_BITS) - 1);

    if ((encoded & (UINT64_C(1) << (LT_SMALL_FRACTION_NUMERATOR_BITS - 1))) != 0){
        encoded |= ~((UINT64_C(1) << LT_SMALL_FRACTION_NUMERATOR_BITS) - 1);
    }
    return (int64_t)encoded;
}

static inline uint32_t LT_SmallFraction_denominator(LT_Value value){
    if (!LT_SmallFraction_p(value)){
        LT_type_error(value, &LT_SmallFraction_class);
    }
    return (uint32_t)(LT_VALUE_IMMEDIATE_VALUE(value)
        & ((UINT64_C(1) << LT_SMALL_FRACTION_DENOMINATOR_BITS) - 1));
}

static inline LT_Value LT_SmallFraction_new(int64_t numerator, uint32_t denominator){
    uint64_t encoded;

    if (!LT_SmallFraction_in_range(numerator, denominator)){
        LT_error("SmallFraction out of range");
    }

    encoded = (uint64_t)numerator
        & ((UINT64_C(1) << LT_SMALL_FRACTION_NUMERATOR_BITS) - 1);

    return LT_VALUE_MAKE_IMMEDIATE(
        LT_VALUE_IMMEDIATE_TAG_SMALL_FRACTION,
        ((encoded << LT_SMALL_FRACTION_DENOMINATOR_BITS)
        | (uint64_t)denominator)
    );
}

LT__END_DECLS

#endif
