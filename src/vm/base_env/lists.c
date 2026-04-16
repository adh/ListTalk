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

static size_t collect_list_arguments(LT_Value lists,
                                     const char* primitive_name,
                                     LT_Value** list_values_out){
    LT_Value list_cursor = lists;
    LT_Value* list_values;
    size_t list_count = 0;

    while (list_cursor != LT_NIL){
        if (!LT_Pair_p(list_cursor)){
            LT_error("Malformed argument list while iterating lists");
        }
        list_count++;
        list_cursor = LT_cdr(list_cursor);
    }

    if (list_count == 0){
        LT_error("%s expects at least one list", primitive_name);
    }

    list_values = GC_MALLOC(sizeof(LT_Value) * list_count);
    list_cursor = lists;
    for (size_t i = 0; i < list_count; i++){
        list_values[i] = LT_car(list_cursor);
        list_cursor = LT_cdr(list_cursor);
    }

    *list_values_out = list_values;
    return list_count;
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
    if (!LT_Pair_p(pair_value)){
        LT_type_error(pair_value, &LT_List_class);
    }
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
    if (!LT_Pair_p(pair_value)){
        LT_type_error(pair_value, &LT_List_class);
    }
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

LT_DEFINE_PRIMITIVE(
    primitive_append,
    "append",
    "(list ...)",
    "Concatenate lists, sharing the final argument tail."
){
    LT_Value cursor = arguments;
    LT_ListBuilder* builder = LT_ListBuilder_new();

    if (cursor == LT_NIL){
        return LT_NIL;
    }

    while (LT_Pair_p(cursor)){
        LT_Value list = LT_car(cursor);
        LT_Value next = LT_cdr(cursor);

        if (next == LT_NIL){
            return LT_ListBuilder_valueWithRest(builder, list);
        }

        while (LT_Pair_p(list)){
            LT_ListBuilder_append(builder, LT_car(list));
            list = LT_cdr(list);
        }
        if (list != LT_NIL){
            LT_error("append expects proper lists before final argument");
        }
        cursor = next;
    }

    LT_error("Malformed argument list while appending lists");
    return LT_NIL;
}

LT_DEFINE_PRIMITIVE(
    primitive_map,
    "map",
    "(callable list list ...)",
    "Return list of callable results for each element tuple, stopping at shortest list."
){
    LT_Value cursor = arguments;
    LT_Value callable;
    LT_Value lists = LT_NIL;
    LT_Value* list_values;
    size_t list_count;

    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_REST(cursor, lists);

    list_count = collect_list_arguments(lists, "map", &list_values);

    return LT_List_map_many(callable, list_count, list_values);
}

LT_DEFINE_PRIMITIVE(
    primitive_for_each,
    "for-each",
    "(callable list list ...)",
    "Apply callable for each element tuple, stopping at shortest list."
){
    LT_Value cursor = arguments;
    LT_Value callable;
    LT_Value lists = LT_NIL;
    LT_Value* list_values;
    size_t list_count;

    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_REST(cursor, lists);

    list_count = collect_list_arguments(lists, "for-each", &list_values);
    LT_List_for_each_many(callable, list_count, list_values);
    return LT_NIL;
}

LT_DEFINE_PRIMITIVE(
    primitive_any,
    "any",
    "(callable list list ...)",
    "Return true when callable returns truthy for any element tuple."
){
    LT_Value cursor = arguments;
    LT_Value callable;
    LT_Value lists = LT_NIL;
    LT_Value* list_values;
    size_t list_count;

    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_REST(cursor, lists);

    list_count = collect_list_arguments(lists, "any", &list_values);
    return LT_List_any_many(callable, list_count, list_values);
}

LT_DEFINE_PRIMITIVE(
    primitive_every,
    "every",
    "(callable list list ...)",
    "Return true when callable returns truthy for every element tuple."
){
    LT_Value cursor = arguments;
    LT_Value callable;
    LT_Value lists = LT_NIL;
    LT_Value* list_values;
    size_t list_count;

    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_REST(cursor, lists);

    list_count = collect_list_arguments(lists, "every", &list_values);
    return LT_List_every_many(callable, list_count, list_values);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    primitive_memq,
    "memq",
    "(value list)",
    "Return list tail whose car is value by identity, else false.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value value;
    LT_Value list;

    LT_OBJECT_ARG(cursor, value);
    LT_OBJECT_ARG(cursor, list);
    LT_ARG_END(cursor);

    while (list != LT_NIL){
        if (!LT_Pair_p(list)){
            LT_error("memq expects proper list");
        }
        if (LT_car(list) == value){
            return list;
        }
        list = LT_cdr(list);
    }

    return LT_FALSE;
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
    LT_base_env_bind_static_primitive(environment, &primitive_append);
    LT_base_env_bind_static_primitive(environment, &primitive_map);
    LT_base_env_bind_static_primitive(environment, &primitive_for_each);
    LT_base_env_bind_static_primitive(environment, &primitive_any);
    LT_base_env_bind_static_primitive(environment, &primitive_every);
    LT_base_env_bind_static_primitive(environment, &primitive_memq);
    LT_base_env_bind_static_primitive(environment, &primitive_assoc);
    LT_base_env_bind_static_primitive(environment, &primitive_assq);
    LT_base_env_bind_static_primitive(environment, &primitive_assv);
}
