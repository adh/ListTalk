/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Vector.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/macros/arg_macros.h>

struct LT_Vector_s {
    LT_Object base;
    size_t length;
    LT_Value items[];
};

static size_t Vector_hash(LT_Value value){
    LT_Vector* vector = LT_Vector_from_value(value);
    size_t length = LT_Vector_length(vector);
    size_t hash = (size_t)0x9e3779b1;
    size_t i;

    for (i = 0; i < length; i++){
        hash = (hash * (size_t)33) ^ LT_Value_hash(vector->items[i]);
    }
    return hash;
}

static int Vector_equal_p(LT_Value left, LT_Value right){
    LT_Vector* left_vector;
    LT_Vector* right_vector;
    size_t i;
    size_t length;

    if (!LT_Vector_p(right)){
        return 0;
    }

    left_vector = LT_Vector_from_value(left);
    right_vector = LT_Vector_from_value(right);
    if (LT_Vector_length(left_vector) != LT_Vector_length(right_vector)){
        return 0;
    }

    length = LT_Vector_length(left_vector);
    for (i = 0; i < length; i++){
        if (!LT_Value_equal_p(left_vector->items[i], right_vector->items[i])){
            return 0;
        }
    }

    return 1;
}

static void Vector_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Vector* vector = LT_Vector_from_value(obj);
    size_t i;

    fputs("#(", stream);
    for (i = 0; i < vector->length; i++){
        if (i != 0){
            fputc(' ', stream);
        }
        LT_Value_debugPrintOn(vector->items[i], stream);
    }
    fputc(')', stream);
}

LT_DEFINE_PRIMITIVE(
    vector_method_length,
    "Vector>>length",
    "(self)",
    "Return vector length."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Number_smallinteger_from_size(
        LT_Vector_length(LT_Vector_from_value(self)),
        "Vector length does not fit fixnum"
    );
}

LT_DEFINE_PRIMITIVE(
    vector_method_at,
    "Vector>>at:",
    "(self index)",
    "Return vector item at index."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value index;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, index);
    LT_ARG_END(cursor);
    return LT_Vector_at(
        LT_Vector_from_value(self),
        LT_Number_nonnegative_size_from_integer(
            index,
            "Vector index out of bounds",
            "Vector index out of bounds"
        )
    );
}

LT_DEFINE_PRIMITIVE(
    vector_method_at_put,
    "Vector>>at:put:",
    "(self index value)",
    "Set vector item at index."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value index;
    LT_Value value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, index);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    LT_Vector_atPut(
        LT_Vector_from_value(self),
        LT_Number_nonnegative_size_from_integer(
            index,
            "Vector index out of bounds",
            "Vector index out of bounds"
        ),
        value
    );
    return value;
}

static LT_Method_Descriptor Vector_methods[] = {
    {"length", &vector_method_length},
    {"at:", &vector_method_at},
    {"at:put:", &vector_method_at_put},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Vector) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Vector",
    .instance_size = sizeof(LT_Vector),
    .class_flags = LT_CLASS_FLAG_FLEXIBLE,
    .hash = Vector_hash,
    .equal_p = Vector_equal_p,
    .debugPrintOn = Vector_debugPrintOn,
    .methods = Vector_methods,
};

LT_Vector* LT_Vector_new(size_t length){
    LT_Vector* vector = LT_Class_ALLOC_FLEXIBLE(LT_Vector, sizeof(LT_Value) * length);
    size_t i;

    vector->length = length;
    for (i = 0; i < length; i++){
        vector->items[i] = LT_NIL;
    }
    return vector;
}

size_t LT_Vector_length(LT_Vector* vector){
    return vector->length;
}

LT_Value LT_Vector_at(LT_Vector* vector, size_t index){
    if (index >= vector->length){
        LT_error("Vector index out of bounds");
    }
    return vector->items[index];
}

void LT_Vector_atPut(LT_Vector* vector, size_t index, LT_Value value){
    if (index >= vector->length){
        LT_error("Vector index out of bounds");
    }
    vector->items[index] = value;
}
