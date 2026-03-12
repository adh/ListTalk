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
#include <string.h>

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

bool LT_Value_equal_p(LT_Value left, LT_Value right){
    if (LT_Value_eqv_p(left, right)){
        return true;
    }

    if (LT_String_p(left) && LT_String_p(right)){
        LT_String* left_string = LT_String_from_value(left);
        LT_String* right_string = LT_String_from_value(right);
        size_t length = LT_String_length(left_string);

        if (length != LT_String_length(right_string)){
            return false;
        }

        return memcmp(
            LT_String_value_cstr(left_string),
            LT_String_value_cstr(right_string),
            length
        ) == 0;
    }

    if (LT_Pair_p(left) && LT_Pair_p(right)){
        return LT_Value_equal_p(LT_car(left), LT_car(right))
            && LT_Value_equal_p(LT_cdr(left), LT_cdr(right));
    }

    if (LT_Vector_p(left) && LT_Vector_p(right)){
        LT_Vector* left_vector = LT_Vector_from_value(left);
        LT_Vector* right_vector = LT_Vector_from_value(right);
        size_t i;
        size_t length;

        if (LT_Vector_length(left_vector) != LT_Vector_length(right_vector)){
            return false;
        }

        length = LT_Vector_length(left_vector);
        for (i = 0; i < length; i++){
            if (!LT_Value_equal_p(
                LT_Vector_at(left_vector, i),
                LT_Vector_at(right_vector, i)
            )){
                return false;
            }
        }

        return true;
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
