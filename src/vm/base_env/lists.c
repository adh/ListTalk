/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/classes/Pair.h>
#include <ListTalk/macros/arg_macros.h>

static LT_Value primitive_cons(LT_Value arguments){
    LT_Value cursor = arguments;
    LT_Value car_value;
    LT_Value cdr_value;

    LT_OBJECT_ARG(cursor, car_value);
    LT_OBJECT_ARG(cursor, cdr_value);
    LT_ARG_END(cursor);
    return LT_cons(car_value, cdr_value);
}

static LT_Value primitive_car(LT_Value arguments){
    LT_Value cursor = arguments;
    LT_Value pair_value;

    LT_OBJECT_ARG(cursor, pair_value);
    LT_ARG_END(cursor);
    LT_Pair_from_value(pair_value);
    return LT_car(pair_value);
}

static LT_Value primitive_cdr(LT_Value arguments){
    LT_Value cursor = arguments;
    LT_Value pair_value;

    LT_OBJECT_ARG(cursor, pair_value);
    LT_ARG_END(cursor);
    LT_Pair_from_value(pair_value);
    return LT_cdr(pair_value);
}

static LT_Value primitive_pair_p(LT_Value arguments){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Pair_p(value) ? LT_TRUE : LT_FALSE;
}

void LT_base_env_bind_lists(LT_Environment* environment){
    LT_base_env_bind_primitive(environment, "cons", primitive_cons);
    LT_base_env_bind_primitive(environment, "car", primitive_car);
    LT_base_env_bind_primitive(environment, "cdr", primitive_cdr);
    LT_base_env_bind_primitive(environment, "pair?", primitive_pair_p);
}
