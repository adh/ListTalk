/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/InexactComplexNumber.h>
#include <ListTalk/classes/ComplexNumber.h>
#include <ListTalk/classes/Float.h>
#include <ListTalk/macros/decl_macros.h>

#include <stdio.h>

struct LT_InexactComplexNumber_s {
    LT_Object base;
    double real;
    double imaginary;
};

static void InexactComplexNumber_debugPrintOn(LT_Value value, FILE* stream){
    LT_InexactComplexNumber* complex = LT_InexactComplexNumber_from_value(value);

    fprintf(
        stream,
        "%.17g%+.17gi",
        complex->real,
        complex->imaginary
    );
}

LT_DEFINE_CLASS(LT_InexactComplexNumber) {
    .superclass = &LT_ComplexNumber_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "InexactComplexNumber",
    .instance_size = sizeof(LT_InexactComplexNumber),
    .class_flags = LT_CLASS_FLAG_IMMUTABLE | LT_CLASS_FLAG_SCALAR,
    .debugPrintOn = InexactComplexNumber_debugPrintOn,
};

LT_Value LT_InexactComplexNumber_new(double real, double imaginary){
    LT_InexactComplexNumber* complex = LT_Class_ALLOC(LT_InexactComplexNumber);

    if (imaginary == 0.0){
        return LT_Float_new(real);
    }

    complex->real = real;
    complex->imaginary = imaginary;
    return (LT_Value)(uintptr_t)complex;
}

double LT_InexactComplexNumber_real(LT_Value value){
    return LT_InexactComplexNumber_from_value(value)->real;
}

double LT_InexactComplexNumber_imaginary(LT_Value value){
    return LT_InexactComplexNumber_from_value(value)->imaginary;
}
