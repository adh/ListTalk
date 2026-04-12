/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/classes/Number.h>

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_add,
    "+",
    "(n ...)",
    "Return sum of numeric arguments.",
    LT_PRIMITIVE_FLAG_PURE
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

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_subtract,
    "-",
    "(n [n ...])",
    "Negate one value or subtract remaining values from first.",
    LT_PRIMITIVE_FLAG_PURE
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

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_multiply,
    "*",
    "(n ...)",
    "Return product of numeric arguments.",
    LT_PRIMITIVE_FLAG_PURE
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

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_divide,
    "/",
    "(n n ...)",
    "Divide first value by remaining values.",
    LT_PRIMITIVE_FLAG_PURE
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

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_less_than,
    "<",
    "(n n ...)",
    "Return true when numeric arguments are in strictly ascending order.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value prev;

    LT_OBJECT_ARG(cursor, prev);
    if (cursor == LT_NIL){
        LT_error("< requires at least two arguments");
    }

    while (cursor != LT_NIL){
        LT_Value next;
        LT_OBJECT_ARG(cursor, next);

        if (LT_Number_compare(prev, next) >= 0){
            return LT_FALSE;
        }
        prev = next;
    }

    return LT_TRUE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_greater_than,
    ">",
    "(n n ...)",
    "Return true when numeric arguments are in strictly descending order.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value prev;

    LT_OBJECT_ARG(cursor, prev);
    if (cursor == LT_NIL){
        LT_error("> requires at least two arguments");
    }

    while (cursor != LT_NIL){
        LT_Value next;
        LT_OBJECT_ARG(cursor, next);

        if (LT_Number_compare(prev, next) <= 0){
            return LT_FALSE;
        }
        prev = next;
    }

    return LT_TRUE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_less_than_or_equal,
    "<=",
    "(n n ...)",
    "Return true when numeric arguments are in non-descending order.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value prev;

    LT_OBJECT_ARG(cursor, prev);
    if (cursor == LT_NIL){
        LT_error("<= requires at least two arguments");
    }

    while (cursor != LT_NIL){
        LT_Value next;
        LT_OBJECT_ARG(cursor, next);

        if (LT_Number_compare(prev, next) > 0){
            return LT_FALSE;
        }
        prev = next;
    }

    return LT_TRUE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_greater_than_or_equal,
    ">=",
    "(n n ...)",
    "Return true when numeric arguments are in non-ascending order.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value prev;

    LT_OBJECT_ARG(cursor, prev);
    if (cursor == LT_NIL){
        LT_error(">= requires at least two arguments");
    }

    while (cursor != LT_NIL){
        LT_Value next;
        LT_OBJECT_ARG(cursor, next);

        if (LT_Number_compare(prev, next) < 0){
            return LT_FALSE;
        }
        prev = next;
    }

    return LT_TRUE;
}

void LT_base_env_bind_numbers(LT_Environment* environment){
    LT_base_env_bind_static_primitive(environment, &primitive_add);
    LT_base_env_bind_static_primitive(environment, &primitive_subtract);
    LT_base_env_bind_static_primitive(environment, &primitive_multiply);
    LT_base_env_bind_static_primitive(environment, &primitive_divide);
    LT_base_env_bind_static_primitive(environment, &primitive_less_than);
    LT_base_env_bind_static_primitive(environment, &primitive_greater_than);
    LT_base_env_bind_static_primitive(environment, &primitive_less_than_or_equal);
    LT_base_env_bind_static_primitive(environment, &primitive_greater_than_or_equal);
}
