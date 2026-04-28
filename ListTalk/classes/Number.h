/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Number__
#define H__ListTalk__Number__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/vm/Class.h>

#include <stdbool.h>
#include <limits.h>
#include <stdint.h>

LT__BEGIN_DECLS

typedef struct LT_Number_s LT_Number;

extern LT_Class LT_Number_class;
extern LT_Class LT_Number_class_class;

static inline size_t LT_Number_nonnegative_size_from_int64(
    int64_t value,
    const char* negative_message,
    const char* range_message
){
    if (value < 0){
        LT_error(negative_message);
    }
    if (!LT_SmallInteger_in_range((int64_t)(size_t)value)){
        LT_error(range_message);
    }
    return (size_t)value;
}

static inline size_t LT_Number_nonnegative_size_from_integer(
    LT_Value value,
    const char* negative_message,
    const char* range_message
){
    return LT_Number_nonnegative_size_from_int64(
        LT_SmallInteger_value(value),
        negative_message,
        range_message
    );
}

static inline LT_Value LT_Number_smallinteger_from_size(
    size_t value,
    const char* range_message
){
    if (value > (size_t)LT_VALUE_FIXNUM_MAX){
        LT_error(range_message);
    }
    return LT_SmallInteger_new((int64_t)value);
}

static inline LT_Value LT_Number_smallinteger_from_long(
    long value,
    const char* range_message
){
    if (value < (long)LT_VALUE_FIXNUM_MIN || value > (long)LT_VALUE_FIXNUM_MAX){
        LT_error(range_message);
    }
    return LT_SmallInteger_new((int64_t)value);
}

static inline uint8_t LT_Number_uint8_from_int64(
    int64_t value,
    const char* range_message
){
    if (value < 0 || value > UINT8_MAX){
        LT_error(range_message);
    }
    return (uint8_t)value;
}

static inline uint8_t LT_Number_uint8_from_integer(
    LT_Value value,
    const char* range_message
){
    return LT_Number_uint8_from_int64(
        LT_SmallInteger_value(value),
        range_message
    );
}

static inline long LT_Number_long_from_integer(
    LT_Value value,
    const char* range_message
){
    int64_t integer = LT_SmallInteger_value(value);

    if (integer < (int64_t)LONG_MIN || integer > (int64_t)LONG_MAX){
        LT_error(range_message);
    }
    return (long)integer;
}

static inline int LT_Number_int_from_integer(
    LT_Value value,
    int64_t min_value,
    int64_t max_value,
    const char* range_message
){
    int64_t integer = LT_SmallInteger_value(value);

    if (integer < min_value || integer > max_value){
        LT_error(range_message);
    }
    return (int)integer;
}

int LT_Number_parse_token_with_radix(const char* token, unsigned int radix, LT_Value* value);
char* LT_Number_to_string(LT_Value value);
double LT_Number_to_double(LT_Value value);
LT_Value LT_Number_add2(LT_Value left, LT_Value right);
LT_Value LT_Number_subtract2(LT_Value left, LT_Value right);
LT_Value LT_Number_multiply2(LT_Value left, LT_Value right);
LT_Value LT_Number_divide2(LT_Value left, LT_Value right);
LT_Value LT_Number_negate(LT_Value value);
LT_Value LT_Number_abs(LT_Value value);
LT_Value LT_Number_phase(LT_Value value);
LT_Value LT_Number_floor(LT_Value value);
LT_Value LT_Number_truncate(LT_Value value);
LT_Value LT_Number_ceiling(LT_Value value);
LT_Value LT_Number_round(LT_Value value);
LT_Value LT_Number_sqrt(LT_Value value);
LT_Value LT_Number_min2(LT_Value left, LT_Value right);
LT_Value LT_Number_max2(LT_Value left, LT_Value right);
bool LT_Number_zero_p(LT_Value value);
bool LT_Number_positive_p(LT_Value value);
bool LT_Number_negative_p(LT_Value value);
LT_Value LT_Number_sin(LT_Value value);
LT_Value LT_Number_cos(LT_Value value);
LT_Value LT_Number_tan(LT_Value value);
LT_Value LT_Number_log(LT_Value value);
LT_Value LT_Number_exp(LT_Value value);
LT_Value LT_Number_expt(LT_Value base, LT_Value exponent);
bool LT_Number_equal_p(LT_Value left, LT_Value right);
int LT_Number_compare(LT_Value left, LT_Value right);

LT__END_DECLS

#endif
