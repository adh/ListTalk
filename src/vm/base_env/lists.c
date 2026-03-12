/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/classes/Pair.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/error.h>

#include <stdbool.h>

static LT_Value assoc_with_predicate(LT_Value key,
                                     LT_Value alist,
                                     bool (*predicate)(LT_Value left, LT_Value right)){
    LT_Value cursor = alist;

    while (cursor != LT_NIL){
        LT_Value entry;

        if (!LT_Pair_p(cursor)){
            LT_error("assoc expects proper list");
        }

        entry = LT_car(cursor);
        if (!LT_Pair_p(entry)){
            LT_error("assoc expects list of pairs");
        }

        if (predicate(key, LT_car(entry))){
            return entry;
        }

        cursor = LT_cdr(cursor);
    }

    return LT_FALSE;
}

static bool value_eq_p(LT_Value left, LT_Value right){
    return left == right;
}

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

LT_DEFINE_PRIMITIVE(
    primitive_list,
    "list",
    "(value ...)",
    "Return a list containing all arguments."
){
    LT_Value cursor = arguments;

    while (cursor != LT_NIL){
        if (!LT_Pair_p(cursor)){
            LT_error("Malformed argument list while creating list");
        }
        cursor = LT_cdr(cursor);
    }

    return arguments;
}

LT_DEFINE_PRIMITIVE(
    primitive_list_p,
    "list?",
    "(value)",
    "Return true when value is a proper list."
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);

    while (LT_Pair_p(value)){
        value = LT_cdr(value);
    }
    return value == LT_NIL ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_assoc,
    "assoc",
    "(key alist)",
    "Return matching pair in alist by structural equality, else false.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value key;
    LT_Value alist;

    LT_OBJECT_ARG(cursor, key);
    LT_OBJECT_ARG(cursor, alist);
    LT_ARG_END(cursor);

    return assoc_with_predicate(key, alist, LT_Value_equal_p);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_assq,
    "assq",
    "(key alist)",
    "Return matching pair in alist by identity equality, else false.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value key;
    LT_Value alist;

    LT_OBJECT_ARG(cursor, key);
    LT_OBJECT_ARG(cursor, alist);
    LT_ARG_END(cursor);

    return assoc_with_predicate(key, alist, value_eq_p);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_assv,
    "assv",
    "(key alist)",
    "Return matching pair in alist by eqv? semantics, else false.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value key;
    LT_Value alist;

    LT_OBJECT_ARG(cursor, key);
    LT_OBJECT_ARG(cursor, alist);
    LT_ARG_END(cursor);

    return assoc_with_predicate(key, alist, LT_Value_eqv_p);
}

void LT_base_env_bind_lists(LT_Environment* environment){
    LT_base_env_bind_static_primitive(environment, &primitive_cons);
    LT_base_env_bind_static_primitive(environment, &primitive_car);
    LT_base_env_bind_static_primitive(environment, &primitive_cdr);
    LT_base_env_bind_static_primitive(environment, &primitive_pair_p);
    LT_base_env_bind_static_primitive(environment, &primitive_list);
    LT_base_env_bind_static_primitive(environment, &primitive_list_p);
    LT_base_env_bind_static_primitive(environment, &primitive_assoc);
    LT_base_env_bind_static_primitive(environment, &primitive_assq);
    LT_base_env_bind_static_primitive(environment, &primitive_assv);
}
