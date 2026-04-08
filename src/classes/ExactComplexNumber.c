/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/ExactComplexNumber.h>
#include <ListTalk/classes/ComplexNumber.h>
#include <ListTalk/classes/Float.h>
#include <ListTalk/classes/Fraction.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/RealNumber.h>
#include <ListTalk/classes/RationalNumber.h>
#include <ListTalk/classes/SmallFraction.h>
#include <ListTalk/classes/SmallInteger.h>
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
    LT_ExactComplexNumber* complex = LT_ExactComplexNumber_from_value(value);
    int64_t imag_small;

    LT_Value_debugPrintOn(complex->real, stream);
    if (LT_Value_is_fixnum(complex->imaginary)
        && (imag_small = LT_SmallInteger_value(complex->imaginary)) >= 0){
        fputc('+', stream);
    } else if (LT_Fraction_p(complex->imaginary)){
        LT_Value numerator = LT_Fraction_numerator(complex->imaginary);
        if (LT_Value_is_fixnum(numerator) && LT_SmallInteger_value(numerator) >= 0){
            fputc('+', stream);
        }
    } else if (LT_SmallFraction_p(complex->imaginary)
        && LT_SmallFraction_numerator(complex->imaginary) >= 0){
        fputc('+', stream);
    }
    LT_Value_debugPrintOn(complex->imaginary, stream);
    fputc('i', stream);
}

static LT_Slot_Descriptor ExactComplexNumber_slots[] = {
    {"real", offsetof(LT_ExactComplexNumber, real), &LT_SlotType_ReadonlyObject},
    {"imaginary", offsetof(LT_ExactComplexNumber, imaginary), &LT_SlotType_ReadonlyObject},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_ExactComplexNumber) {
    .superclass = &LT_ComplexNumber_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "ExactComplexNumber",
    .instance_size = sizeof(LT_ExactComplexNumber),
    .class_flags = LT_CLASS_FLAG_IMMUTABLE | LT_CLASS_FLAG_SCALAR,
    .debugPrintOn = ExactComplexNumber_debugPrintOn,
    .slots = ExactComplexNumber_slots,
};

LT_Value LT_ExactComplexNumber_new(LT_Value real, LT_Value imaginary){
    LT_ExactComplexNumber* complex;

    if (!LT_Value_is_instance_of(real, LT_STATIC_CLASS(LT_RationalNumber))){
        LT_type_error(real, &LT_RationalNumber_class);
    }
    if (!LT_Value_is_instance_of(imaginary, LT_STATIC_CLASS(LT_RationalNumber))){
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
