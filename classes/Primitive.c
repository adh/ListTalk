/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Primitive.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/utils.h>

struct LT_Primitive_s {
    LT_Primitive_Func function;
    char* name;
};

static void Primitive_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Primitive* primitive = LT_Primitive_from_object(obj);
    if (primitive->name == NULL){
        fputs("#<Primitive>", stream);
        return;
    }
    fputs("#<Primitive ", stream);
    fputs(primitive->name, stream);
    fputc('>', stream);
}

LT_DEFINE_CLASS(LT_Primitive) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Primitive",
    .instance_size = sizeof(LT_Primitive),
    .class_flags = LT_CLASS_FLAG_SPECIAL,
    .debugPrintOn = Primitive_debugPrintOn,
};

LT_Value LT_Primitive_new(char* name, LT_Primitive_Func function){
    LT_Primitive* primitive = GC_NEW(LT_Primitive);
    primitive->function = function;
    if (name == NULL){
        primitive->name = NULL;
    } else {
        primitive->name = LT_strdup(name);
    }
    return ((LT_Value)(uintptr_t)primitive) | LT_VALUE_POINTER_TAG_PRIMITIVE;
}

char* LT_Primitive_name(LT_Primitive* primitive){
    return primitive->name;
}

LT_Primitive_Func LT_Primitive_function(LT_Primitive* primitive){
    return primitive->function;
}

LT_Value LT_Primitive_call(LT_Value primitive, LT_Value arguments){
    LT_Primitive_Func function = LT_Primitive_function(
        LT_Primitive_from_object(primitive)
    );
    return function(arguments);
}
