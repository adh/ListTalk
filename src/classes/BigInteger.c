/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/BigInteger.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/macros/decl_macros.h>

#include <stdio.h>

struct LT_BigInteger_s {
    LT_Object base;
    __int128 value;
};

static void BigInteger_debugPrintOn(LT_Value value, FILE* stream){
    char* text = LT_BigInteger_to_decimal_cstr(value);
    fputs(text, stream);
}

LT_DEFINE_CLASS(LT_BigInteger) {
    .superclass = &LT_Number_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "BigInteger",
    .instance_size = sizeof(LT_BigInteger),
    .class_flags = LT_CLASS_FLAG_IMMUTABLE | LT_CLASS_FLAG_SCALAR,
    .debugPrintOn = BigInteger_debugPrintOn,
};
