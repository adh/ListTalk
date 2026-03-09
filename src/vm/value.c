/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/vm/value.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/classes/Boolean.h>
#include <ListTalk/classes/Nil.h>
#include <ListTalk/classes/Float.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/classes/Closure.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Macro.h>
#include <ListTalk/classes/SpecialForm.h>

#include <inttypes.h>

LT_Class* const LT__Immediate_classes[64] = {
    [LT_VALUE_IMMEDIATE_TAG_BOOLEAN & 0x3f] = &LT_Boolean_class,
    [LT_VALUE_IMMEDIATE_TAG_NIL & 0x3f] = &LT_Nil_class,
    [LT_VALUE_IMMEDIATE_TAG_FIXNUM & 0x3f] = &LT_SmallInteger_class,
};
LT_Class* const LT__Pointer_classes[8] = {
    &LT_Class_class,
    &LT_Class_class,
    &LT_Pair_class,
    &LT_Symbol_class,
    &LT_Closure_class,
    &LT_Primitive_class,
    &LT_Macro_class,
    &LT_SpecialForm_class
};

void LT_Value_debugPrintOn(LT_Value value, FILE* stream){
    LT_Class* klass = LT_Value_class(value);
    char* class_name = "value";

    if (klass != NULL && klass->debugPrintOn != NULL){
        klass->debugPrintOn(value, stream);
        return;
    }

    if (klass != NULL && LT_Symbol_p(klass->name)){
        class_name = LT_Symbol_name(LT_Symbol_from_value(klass->name));
    }

    fprintf(
        stream,
        "#<%s at 0x%" PRIxPTR ">",
        class_name,
        (uintptr_t)value
    );
}

void* LT_Class_alloc(LT_Class *klass)
{
    LT_Object* obj = GC_MALLOC(klass->instance_size);
    obj->klass = klass;
    return obj;
}
void* LT_Class_alloc_flexible(LT_Class *klass, size_t flex)
{
    LT_Object* obj = GC_MALLOC(klass->instance_size + flex);
    obj->klass = klass;
    return obj;
}
