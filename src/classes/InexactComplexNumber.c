/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/InexactComplexNumber.h>
#include <ListTalk/classes/ComplexNumber.h>
#include <ListTalk/classes/Float.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/macros/decl_macros.h>

#include <stddef.h>
#include <stdio.h>

struct LT_InexactComplexNumber_s {
    LT_Object base;
    LT_Value real;
    LT_Value imaginary;
};

static void InexactComplexNumber_debugPrintOn(LT_Value value, FILE* stream){
    fputs(LT_Number_to_string(value), stream);
}

static LT_Slot_Descriptor InexactComplexNumber_slots[] = {
    {"real", offsetof(LT_InexactComplexNumber, real), &LT_SlotType_ReadonlyObject},
    {"imaginary", offsetof(LT_InexactComplexNumber, imaginary), &LT_SlotType_ReadonlyObject},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

LT_DEFINE_PRIMITIVE_FLAGS(
    inexactComplexNumber_method_real,
    "InexactComplexNumber>>real",
    "(self)",
    "Return receiver's real component.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_InexactComplexNumber_from_value(self)->real;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    inexactComplexNumber_method_imaginary,
    "InexactComplexNumber>>imaginary",
    "(self)",
    "Return receiver's imaginary component.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_InexactComplexNumber_from_value(self)->imaginary;
}

static LT_Method_Descriptor InexactComplexNumber_methods[] = {
    {"real", &inexactComplexNumber_method_real},
    {"imaginary", &inexactComplexNumber_method_imaginary},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_InexactComplexNumber) {
    .superclass = &LT_ComplexNumber_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "InexactComplexNumber",
    .instance_size = sizeof(LT_InexactComplexNumber),
    .class_flags = LT_CLASS_FLAG_IMMUTABLE | LT_CLASS_FLAG_SCALAR,
    .debugPrintOn = InexactComplexNumber_debugPrintOn,
    .slots = InexactComplexNumber_slots,
    .methods = InexactComplexNumber_methods,
};

LT_Value LT_InexactComplexNumber_new(double real, double imaginary){
    LT_InexactComplexNumber* complex = LT_Class_ALLOC(LT_InexactComplexNumber);

    if (imaginary == 0.0){
        return LT_Float_new(real);
    }

    complex->real = LT_Float_new(real);
    complex->imaginary = LT_Float_new(imaginary);
    return (LT_Value)(uintptr_t)complex;
}

double LT_InexactComplexNumber_real(LT_Value value){
    return LT_Float_value(LT_InexactComplexNumber_from_value(value)->real);
}

double LT_InexactComplexNumber_imaginary(LT_Value value){
    return LT_Float_value(LT_InexactComplexNumber_from_value(value)->imaginary);
}
