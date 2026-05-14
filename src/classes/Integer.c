/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Integer.h>
#include <ListTalk/classes/BigInteger.h>
#include <ListTalk/classes/RationalNumber.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/macros/decl_macros.h>

#include <stddef.h>

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
    uintmax_t magnitude;
    uint32_t limbs[sizeof(uintmax_t) / sizeof(uint32_t)];
    size_t limb_count = 0;
    int negative = value < 0;

    if (value >= (intmax_t)LT_VALUE_FIXNUM_MIN
        && value <= (intmax_t)LT_VALUE_FIXNUM_MAX){
        return LT_SmallInteger_new((int64_t)value);
    }

    magnitude = negative
        ? (uintmax_t)(-(value + 1)) + 1
        : (uintmax_t)value;

    while (magnitude != 0){
        limbs[limb_count++] = (uint32_t)magnitude;
        magnitude >>= 32;
    }

    return LT_BigInteger_new_from_limbs(negative, limb_count, limbs);
}

LT_Value LT_Integer_from_uintmax(uintmax_t value){
    uint32_t limbs[sizeof(uintmax_t) / sizeof(uint32_t)];
    size_t limb_count = 0;

    if (value <= (uintmax_t)LT_VALUE_FIXNUM_MAX){
        return LT_SmallInteger_new((int64_t)value);
    }

    while (value != 0){
        limbs[limb_count++] = (uint32_t)value;
        value >>= 32;
    }

    return LT_BigInteger_new_from_limbs(0, limb_count, limbs);
}
