/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/ExactComplexNumber.h>
#include <ListTalk/classes/ComplexNumber.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/RealNumber.h>
#include <ListTalk/classes/RationalNumber.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/vm/error.h>

#include <stddef.h>
#include <stdio.h>

struct LT_ExactComplexNumber_s {
    LT_Object base;
    LT_Value real;
    LT_Value imaginary;
};

static void ExactComplexNumber_debugPrintOn(LT_Value value, FILE* stream){
    fputs(LT_Number_to_string(value), stream);
}

static LT_Slot_Descriptor ExactComplexNumber_slots[] = {
    {"real", offsetof(LT_ExactComplexNumber, real), &LT_SlotType_ReadonlyObject},
    {"imaginary", offsetof(LT_ExactComplexNumber, imaginary), &LT_SlotType_ReadonlyObject},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

LT_DEFINE_PRIMITIVE_FLAGS(
    exactComplexNumber_method_real,
    "ExactComplexNumber>>real",
    "(self)",
    "Return receiver's real component.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_ExactComplexNumber_real(self);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    exactComplexNumber_method_imaginary,
    "ExactComplexNumber>>imaginary",
    "(self)",
    "Return receiver's imaginary component.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_ExactComplexNumber_imaginary(self);
}

static LT_Method_Descriptor ExactComplexNumber_methods[] = {
    {"real", &exactComplexNumber_method_real},
    {"imaginary", &exactComplexNumber_method_imaginary},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_ExactComplexNumber) {
    .superclass = &LT_ComplexNumber_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "ExactComplexNumber",
    .instance_size = sizeof(LT_ExactComplexNumber),
    .class_flags = LT_CLASS_FLAG_IMMUTABLE | LT_CLASS_FLAG_SCALAR,
    .debugPrintOn = ExactComplexNumber_debugPrintOn,
    .slots = ExactComplexNumber_slots,
    .methods = ExactComplexNumber_methods,
};

LT_Value LT_ExactComplexNumber_new(LT_Value real, LT_Value imaginary){
    LT_ExactComplexNumber* complex;

    if (!LT_RationalNumber_value_p(real)){
        LT_type_error(real, &LT_RationalNumber_class);
    }
    if (!LT_RationalNumber_value_p(imaginary)){
        LT_type_error(imaginary, &LT_RationalNumber_class);
    }
    if (LT_Number_equal_p(imaginary, LT_SmallInteger_new(0))){
        return real;
    }

    complex = LT_Class_ALLOC(LT_ExactComplexNumber);
    complex->real = real;
    complex->imaginary = imaginary;
    return (LT_Value)(uintptr_t)complex;
}

LT_Value LT_ExactComplexNumber_real(LT_Value value){
    return LT_ExactComplexNumber_from_value(value)->real;
}

LT_Value LT_ExactComplexNumber_imaginary(LT_Value value){
    return LT_ExactComplexNumber_from_value(value)->imaginary;
}
