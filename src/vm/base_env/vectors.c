/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Vector.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/error.h>

static size_t checked_nonnegative_from_fixnum(int64_t value, const char* primitive_name){
    (void)primitive_name;
    if (value < 0){
        LT_error("Expected non-negative fixnum");
    }
    return (size_t)value;
}

static LT_Value primitive_vector_p(LT_Value arguments){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Vector_p(value) ? LT_TRUE : LT_FALSE;
}

static LT_Value primitive_vector_length(LT_Value arguments){
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

static LT_Value primitive_vector_ref(LT_Value arguments){
    LT_Value cursor = arguments;
    LT_Vector* vector;
    int64_t index_value;

    LT_GENERIC_ARG(cursor, vector, LT_Vector*, LT_Vector_from_value);
    LT_FIXNUM_ARG(cursor, index_value);
    LT_ARG_END(cursor);

    return LT_Vector_at(
        vector,
        checked_nonnegative_from_fixnum(index_value, "vector-ref")
    );
}

static LT_Value primitive_vector_set_bang(LT_Value arguments){
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
        checked_nonnegative_from_fixnum(index_value, "vector-set!"),
        value
    );
    return value;
}

static LT_Value primitive_make_vector(LT_Value arguments){
    LT_Value cursor = arguments;
    int64_t length_value;
    LT_Value fill_value = LT_NIL;
    LT_Vector* vector;
    size_t i;
    size_t length;

    LT_FIXNUM_ARG(cursor, length_value);
    LT_OBJECT_ARG_OPT(cursor, fill_value, LT_NIL);
    LT_ARG_END(cursor);

    length = checked_nonnegative_from_fixnum(length_value, "make-vector");
    vector = LT_Vector_new(length);
    for (i = 0; i < length; i++){
        LT_Vector_atPut(vector, i, fill_value);
    }
    return (LT_Value)(uintptr_t)vector;
}

static LT_Value primitive_vector(LT_Value arguments){
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
    LT_base_env_bind_primitive(environment, "vector?", primitive_vector_p);
    LT_base_env_bind_primitive(environment, "vector-length", primitive_vector_length);
    LT_base_env_bind_primitive(environment, "vector-ref", primitive_vector_ref);
    LT_base_env_bind_primitive(environment, "vector-set!", primitive_vector_set_bang);
    LT_base_env_bind_primitive(environment, "make-vector", primitive_make_vector);
    LT_base_env_bind_primitive(environment, "vector", primitive_vector);
}
