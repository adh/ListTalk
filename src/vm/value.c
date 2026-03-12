/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/vm/value.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/classes/Boolean.h>
#include <ListTalk/classes/Nil.h>
#include <ListTalk/classes/Character.h>
#include <ListTalk/classes/Float.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/classes/Closure.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Macro.h>
#include <ListTalk/classes/SpecialForm.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Vector.h>

#include <inttypes.h>
#include <stdbool.h>

LT_Class* const LT__Immediate_classes[64] = {
    [LT_VALUE_IMMEDIATE_TAG_BOOLEAN & 0x3f] = &LT_Boolean_class,
    [LT_VALUE_IMMEDIATE_TAG_NIL & 0x3f] = &LT_Nil_class,
    [LT_VALUE_IMMEDIATE_TAG_FIXNUM & 0x3f] = &LT_SmallInteger_class,
    [LT_VALUE_IMMEDIATE_TAG_CHARACTER & 0x3f] = &LT_Character_class,
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

bool LT_Value_eqv_p(LT_Value left, LT_Value right){
    if (left == right){
        return true;
    }
    return LT_Number_equal_p(left, right);
}

size_t LT_Value_hash(LT_Value value){
    LT_Class* klass = LT_Value_class(value);

    if (klass != NULL && klass->hash != NULL){
        return klass->hash(value);
    }

    return LT_pointer_hash((void*)(uintptr_t)value);
}

bool LT_Value_equal_p(LT_Value left, LT_Value right){
    LT_Class* left_class;
    LT_Class* right_class;

    if (LT_Value_eqv_p(left, right)){
        return true;
    }

    left_class = LT_Value_class(left);
    right_class = LT_Value_class(right);

    if (left_class != NULL
        && left_class->equal_p != NULL
        && left_class->equal_p(left, right)){
        return true;
    }

    if (right_class != NULL
        && right_class != left_class
        && right_class->equal_p != NULL
        && right_class->equal_p(right, left)){
        return true;
    }

    return false;
}

bool LT_Value_is_instance_of(LT_Value value, LT_Value class_value){
    LT_Class* value_class = LT_Value_class(value);
    LT_Class* expected = LT_Class_from_object(class_value);
    LT_Value expected_value = (LT_Value)(uintptr_t)expected;
    size_t i;

    if (value_class == NULL || value_class->precedence_list == NULL){
        return false;
    }

    for (i = 0; value_class->precedence_list[i] != LT_INVALID; i++){
        if (value_class->precedence_list[i] == expected_value){
            return true;
        }
    }

    return false;
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
