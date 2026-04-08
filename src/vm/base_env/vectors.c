/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Vector.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/error.h>

LT_DEFINE_PRIMITIVE(
    primitive_vector_p,
    "vector?",
    "(value)",
    "Return true when value is a vector."
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Vector_p(value) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    primitive_vector_length,
    "vector-length",
    "(vector)",
    "Return vector length as fixnum."
){
    LT_Value cursor = arguments;
    LT_Vector* vector;
    size_t length;

    LT_GENERIC_ARG(cursor, vector, LT_Vector*, LT_Vector_from_value);
    LT_ARG_END(cursor);

    length = LT_Vector_length(vector);
    if (!LT_SmallInteger_in_range((int64_t)length)){
        LT_error("Vector length does not fit fixnum");
    }
    return LT_SmallInteger_new((int64_t)length);
}

LT_DEFINE_PRIMITIVE(
    primitive_vector_ref,
    "vector-ref",
    "(vector index)",
    "Return value at index."
){
    LT_Value cursor = arguments;
    LT_Vector* vector;
    int64_t index_value;

    LT_GENERIC_ARG(cursor, vector, LT_Vector*, LT_Vector_from_value);
    LT_FIXNUM_ARG(cursor, index_value);
    LT_ARG_END(cursor);

    return LT_Vector_at(
        vector,
        checked_nonnegative_from_fixnum(index_value)
    );
}

LT_DEFINE_PRIMITIVE(
    primitive_vector_set_bang,
    "vector-set!",
    "(vector index value)",
    "Set vector element and return value."
){
    LT_Value cursor = arguments;
    LT_Vector* vector;
    int64_t index_value;
    LT_Value value;

    LT_GENERIC_ARG(cursor, vector, LT_Vector*, LT_Vector_from_value);
    LT_FIXNUM_ARG(cursor, index_value);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);

    LT_Vector_atPut(
        vector,
        checked_nonnegative_from_fixnum(index_value),
        value
    );
    return value;
}

LT_DEFINE_PRIMITIVE(
    primitive_make_vector,
    "make-vector",
    "(length [fill])",
    "Create vector with optional fill value."
){
    LT_Value cursor = arguments;
    int64_t length_value;
    LT_Value fill_value = LT_NIL;
    LT_Vector* vector;
    size_t i;
    size_t length;

    LT_FIXNUM_ARG(cursor, length_value);
    LT_OBJECT_ARG_OPT(cursor, fill_value, LT_NIL);
    LT_ARG_END(cursor);

    length = checked_nonnegative_from_fixnum(length_value);
    vector = LT_Vector_new(length);
    for (i = 0; i < length; i++){
        LT_Vector_atPut(vector, i, fill_value);
    }
    return (LT_Value)(uintptr_t)vector;
}

LT_DEFINE_PRIMITIVE(
    primitive_vector,
    "vector",
    "(value ...)",
    "Create vector from argument values."
){
    LT_Value cursor = arguments;
    LT_Value values;
    size_t length = 0;
    size_t index = 0;
    LT_Vector* vector;

    LT_ARG_REST(cursor, values);
    while (cursor != LT_NIL){
        if (!LT_Pair_p(cursor)){
            LT_error("Malformed argument list while creating vector");
        }
        length++;
        cursor = LT_cdr(cursor);
    }

    vector = LT_Vector_new(length);
    cursor = values;
    while (cursor != LT_NIL){
        LT_Vector_atPut(vector, index, LT_car(cursor));
        cursor = LT_cdr(cursor);
        index++;
    }

    return (LT_Value)(uintptr_t)vector;
}

void LT_base_env_bind_vectors(LT_Environment* environment){
    LT_base_env_bind_static_primitive(environment, &primitive_vector_p);
    LT_base_env_bind_static_primitive(environment, &primitive_vector_length);
    LT_base_env_bind_static_primitive(environment, &primitive_vector_ref);
    LT_base_env_bind_static_primitive(environment, &primitive_vector_set_bang);
    LT_base_env_bind_static_primitive(environment, &primitive_make_vector);
    LT_base_env_bind_static_primitive(environment, &primitive_vector);
}
