/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/classes/Pair.h>
#include <ListTalk/macros/arg_macros.h>

LT_DEFINE_PRIMITIVE(
    primitive_cons,
    "cons",
    "(car cdr)",
    "Construct a pair from car and cdr."
){
    LT_Value cursor = arguments;
    LT_Value car_value;
    LT_Value cdr_value;

    LT_OBJECT_ARG(cursor, car_value);
    LT_OBJECT_ARG(cursor, cdr_value);
    LT_ARG_END(cursor);
    return LT_cons(car_value, cdr_value);
}

LT_DEFINE_PRIMITIVE(
    primitive_car,
    "car",
    "(pair)",
    "Return the car of pair."
){
    LT_Value cursor = arguments;
    LT_Value pair_value;

    LT_OBJECT_ARG(cursor, pair_value);
    LT_ARG_END(cursor);
    LT_Pair_from_value(pair_value);
    return LT_car(pair_value);
}

LT_DEFINE_PRIMITIVE(
    primitive_cdr,
    "cdr",
    "(pair)",
    "Return the cdr of pair."
){
    LT_Value cursor = arguments;
    LT_Value pair_value;

    LT_OBJECT_ARG(cursor, pair_value);
    LT_ARG_END(cursor);
    LT_Pair_from_value(pair_value);
    return LT_cdr(pair_value);
}

LT_DEFINE_PRIMITIVE(
    primitive_pair_p,
    "pair?",
    "(value)",
    "Return true when value is a pair."
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Pair_p(value) ? LT_TRUE : LT_FALSE;
}

void LT_base_env_bind_lists(LT_Environment* environment){
    LT_base_env_bind_static_primitive(environment, &primitive_cons);
    LT_base_env_bind_static_primitive(environment, &primitive_car);
    LT_base_env_bind_static_primitive(environment, &primitive_cdr);
    LT_base_env_bind_static_primitive(environment, &primitive_pair_p);
}
