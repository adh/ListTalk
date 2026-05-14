/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Integer.h>
#include <ListTalk/classes/BigInteger.h>
#include <ListTalk/classes/RationalNumber.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/macros/decl_macros.h>

#include <inttypes.h>
#include <stdio.h>

struct LT_Integer_s {
    LT_Object base;
};

LT_DEFINE_CLASS(LT_Integer) {
    .superclass = &LT_RationalNumber_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Integer",
    .instance_size = sizeof(LT_Integer),
    .class_flags = LT_CLASS_FLAG_ABSTRACT
        | LT_CLASS_FLAG_IMMUTABLE
        | LT_CLASS_FLAG_SCALAR,
};

LT_Value LT_Integer_from_intmax(intmax_t value){
    char buffer[64];

    if (value >= (intmax_t)LT_VALUE_FIXNUM_MIN
        && value <= (intmax_t)LT_VALUE_FIXNUM_MAX){
        return LT_SmallInteger_new((int64_t)value);
    }

    snprintf(buffer, sizeof(buffer), "%" PRIdMAX, value);
    return LT_BigInteger_new_from_digits(buffer, 10);
}

LT_Value LT_Integer_from_uintmax(uintmax_t value){
    char buffer[64];

    if (value <= (uintmax_t)LT_VALUE_FIXNUM_MAX){
        return LT_SmallInteger_new((int64_t)value);
    }

    snprintf(buffer, sizeof(buffer), "%" PRIuMAX, value);
    return LT_BigInteger_new_from_digits(buffer, 10);
}
