/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/classes/Number.h>

LT_DEFINE_PRIMITIVE(
    primitive_add,
    "+",
    "(n ...)",
    "Return sum of numeric arguments."
){
    LT_Value cursor = arguments;
    LT_Value result = LT_SmallInteger_new(0);

    while (cursor != LT_NIL){
        LT_Value operand;
        LT_OBJECT_ARG(cursor, operand);
        result = LT_Number_add2(result, operand);
    }

    return result;
}

LT_DEFINE_PRIMITIVE(
    primitive_subtract,
    "-",
    "(n [n ...])",
    "Negate one value or subtract remaining values from first."
){
    LT_Value cursor = arguments;
    LT_Value result;

    LT_OBJECT_ARG(cursor, result);
    if (cursor == LT_NIL){
        return LT_Number_negate(result);
    }

    while (cursor != LT_NIL){
        LT_Value operand;
        LT_OBJECT_ARG(cursor, operand);
        result = LT_Number_subtract2(result, operand);
    }

    return result;
}

LT_DEFINE_PRIMITIVE(
    primitive_multiply,
    "*",
    "(n ...)",
    "Return product of numeric arguments."
){
    LT_Value cursor = arguments;
    LT_Value result = LT_SmallInteger_new(1);

    while (cursor != LT_NIL){
        LT_Value operand;
        LT_OBJECT_ARG(cursor, operand);
        result = LT_Number_multiply2(result, operand);
    }

    return result;
}

LT_DEFINE_PRIMITIVE(
    primitive_divide,
    "/",
    "(n n ...)",
    "Divide first value by remaining values."
){
    LT_Value cursor = arguments;
    LT_Value result;

    LT_OBJECT_ARG(cursor, result);
    if (cursor == LT_NIL){
        LT_error("Primitive / expects at least two arguments");
    }

    while (cursor != LT_NIL){
        LT_Value operand;
        LT_OBJECT_ARG(cursor, operand);
        result = LT_Number_divide2(result, operand);
    }

    return result;
}

void LT_base_env_bind_numbers(LT_Environment* environment){
    LT_base_env_bind_static_primitive(environment, &primitive_add);
    LT_base_env_bind_static_primitive(environment, &primitive_subtract);
    LT_base_env_bind_static_primitive(environment, &primitive_multiply);
    LT_base_env_bind_static_primitive(environment, &primitive_divide);
}
