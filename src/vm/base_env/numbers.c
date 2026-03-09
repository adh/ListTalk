/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/error.h>

static LT_Value checked_fixnum_from_i128(__int128 result){
    if (result < LT_VALUE_FIXNUM_MIN || result > LT_VALUE_FIXNUM_MAX){
        LT_error("Fixnum arithmetic overflow");
    }

    return LT_Value_fixnum((int64_t)result);
}

static LT_Value primitive_add(LT_Value arguments){
    __int128 sum = 0;
    LT_Value cursor = arguments;

    while (cursor != LT_NIL){
        int64_t arg_value;
        LT_FIXNUM_ARG(cursor, arg_value);
        sum += arg_value;
    }

    return checked_fixnum_from_i128(sum);
}

static LT_Value primitive_subtract(LT_Value arguments){
    LT_Value cursor = arguments;
    int64_t first_value;
    __int128 result;

    LT_FIXNUM_ARG(cursor, first_value);
    result = first_value;

    if (cursor == LT_NIL){
        result = -result;
        return checked_fixnum_from_i128(result);
    }

    while (cursor != LT_NIL){
        int64_t arg_value;
        LT_FIXNUM_ARG(cursor, arg_value);
        result -= arg_value;
    }

    return checked_fixnum_from_i128(result);
}

static LT_Value primitive_multiply(LT_Value arguments){
    __int128 product = 1;
    LT_Value cursor = arguments;

    while (cursor != LT_NIL){
        int64_t arg_value;
        LT_FIXNUM_ARG(cursor, arg_value);
        product *= arg_value;
        if (product < LT_VALUE_FIXNUM_MIN || product > LT_VALUE_FIXNUM_MAX){
            LT_error("Fixnum arithmetic overflow");
        }
    }

    return checked_fixnum_from_i128(product);
}

static LT_Value primitive_divide(LT_Value arguments){
    LT_Value cursor = arguments;
    int64_t first_value;
    __int128 result;

    LT_FIXNUM_ARG(cursor, first_value);
    result = first_value;

    if (cursor == LT_NIL){
        LT_error("Primitive / expects at least two arguments");
    }

    while (cursor != LT_NIL){
        int64_t divisor;
        LT_FIXNUM_ARG(cursor, divisor);
        if (divisor == 0){
            LT_error("Division by zero");
        }
        result /= divisor;
    }

    return checked_fixnum_from_i128(result);
}

void LT_base_env_bind_numbers(LT_Environment* environment){
    LT_base_env_bind_primitive(environment, "+", primitive_add);
    LT_base_env_bind_primitive(environment, "-", primitive_subtract);
    LT_base_env_bind_primitive(environment, "*", primitive_multiply);
    LT_base_env_bind_primitive(environment, "/", primitive_divide);
}
