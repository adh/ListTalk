/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Float.h>

#include <math.h>

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

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_abs,
    "abs",
    "(x)",
    "Return magnitude of a number.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Number_abs(value);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_phase,
    "phase",
    "(x)",
    "Return principal phase angle of a number.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Number_phase(value);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_floor,
    "floor",
    "(x)",
    "Return greatest integer not greater than a real number.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Number_floor(value);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_truncate,
    "truncate",
    "(x)",
    "Return a real number truncated toward zero as an integer.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Number_truncate(value);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_ceiling,
    "ceiling",
    "(x)",
    "Return least integer not less than a real number.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Number_ceiling(value);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_round,
    "round",
    "(x)",
    "Return nearest integer to a real number, rounding halfway values away from zero.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Number_round(value);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_sin,
    "sin",
    "(x)",
    "Return sine of a real number.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Number_sin(value);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_cos,
    "cos",
    "(x)",
    "Return cosine of a real number.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Number_cos(value);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_tan,
    "tan",
    "(x)",
    "Return tangent of a real number.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Number_tan(value);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_log,
    "log",
    "(x)",
    "Return natural logarithm of a real number.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Number_log(value);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_expt,
    "expt",
    "([base [exponent]])",
    "Raise base to exponent, treat one argument as e^x, and return e with no arguments.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value first = LT_NIL;
    LT_Value second = LT_NIL;

    LT_OBJECT_ARG_OPT(cursor, first, LT_NIL);
    LT_OBJECT_ARG_OPT(cursor, second, LT_NIL);
    LT_ARG_END(cursor);

    if (first == LT_NIL){
        return LT_Float_new(exp(1.0));
    }
    if (second == LT_NIL){
        return LT_Number_exp(first);
    }
    return LT_Number_expt(first, second);
}

void LT_base_env_bind_numbers(LT_Environment* environment){
    LT_Environment_bind(
        environment,
        LT_Symbol_new_in(LT_PACKAGE_LISTTALK, "pi"),
        LT_Float_new(acos(-1.0)),
        LT_ENV_BINDING_FLAG_CONSTANT
    );
    LT_base_env_bind_static_primitive(environment, &primitive_add);
    LT_base_env_bind_static_primitive(environment, &primitive_subtract);
    LT_base_env_bind_static_primitive(environment, &primitive_multiply);
    LT_base_env_bind_static_primitive(environment, &primitive_divide);
    LT_base_env_bind_static_primitive(environment, &primitive_less_than);
    LT_base_env_bind_static_primitive(environment, &primitive_greater_than);
    LT_base_env_bind_static_primitive(environment, &primitive_less_than_or_equal);
    LT_base_env_bind_static_primitive(environment, &primitive_greater_than_or_equal);
    LT_base_env_bind_static_primitive(environment, &primitive_abs);
    LT_base_env_bind_static_primitive(environment, &primitive_phase);
    LT_base_env_bind_static_primitive(environment, &primitive_floor);
    LT_base_env_bind_static_primitive(environment, &primitive_truncate);
    LT_base_env_bind_static_primitive(environment, &primitive_ceiling);
    LT_base_env_bind_static_primitive(environment, &primitive_round);
    LT_base_env_bind_static_primitive(environment, &primitive_sin);
    LT_base_env_bind_static_primitive(environment, &primitive_cos);
    LT_base_env_bind_static_primitive(environment, &primitive_tan);
    LT_base_env_bind_static_primitive(environment, &primitive_log);
    LT_base_env_bind_static_primitive(environment, &primitive_expt);
}
