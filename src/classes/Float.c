/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Float.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/RealNumber.h>
#include <ListTalk/macros/decl_macros.h>

#include <stdio.h>

struct LT_Float_s {
    LT_Object base;
};

static void Float_debugPrintOn(LT_Value obj, FILE* stream){
    fputs(LT_Number_to_string(obj), stream);
}

LT_DEFINE_CLASS(LT_Float) {
    .superclass = &LT_RealNumber_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Float",
    .instance_size = sizeof(LT_Float),
    .class_flags = LT_CLASS_FLAG_SPECIAL | LT_CLASS_FLAG_IMMUTABLE
        | LT_CLASS_FLAG_SCALAR,
    .debugPrintOn = Float_debugPrintOn,
};
