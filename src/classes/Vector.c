/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Vector.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>

struct LT_Vector_s {
    LT_Object base;
    size_t length;
    LT_Value items[];
};

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

LT_DEFINE_CLASS(LT_Vector) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Vector",
    .instance_size = sizeof(LT_Vector),
    .class_flags = LT_CLASS_FLAG_FLEXIBLE,
    .debugPrintOn = Vector_debugPrintOn,
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
