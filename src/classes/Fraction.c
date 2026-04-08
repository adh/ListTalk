/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Fraction.h>
#include <ListTalk/classes/BigInteger.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/RationalNumber.h>
#include <ListTalk/classes/SmallFraction.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/macros/decl_macros.h>

#include <stdio.h>

struct LT_Fraction_s {
    LT_Object base;
    LT_Value numerator;
    LT_Value denominator;
};

static void Fraction_debugPrintOn(LT_Value value, FILE* stream){
    LT_Value numerator = LT_Fraction_numerator(value);
    LT_Value denominator = LT_Fraction_denominator(value);

    LT_Value_debugPrintOn(numerator, stream);
    fputc('/', stream);
    LT_Value_debugPrintOn(denominator, stream);
}

LT_DEFINE_CLASS(LT_Fraction) {
    .superclass = &LT_RationalNumber_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Fraction",
    .instance_size = sizeof(LT_Fraction),
    .class_flags = LT_CLASS_FLAG_IMMUTABLE | LT_CLASS_FLAG_SCALAR,
    .debugPrintOn = Fraction_debugPrintOn,
};
