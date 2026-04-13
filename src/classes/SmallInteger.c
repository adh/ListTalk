/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Integer.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/vm/Class.h>

struct LT_SmallInteger_s {
    LT_Object base;
};

static void SmallInteger_debugPrintOn(LT_Value obj, FILE* stream){
    fputs(LT_Number_to_string(obj), stream);
}

LT_DEFINE_CLASS(LT_SmallInteger) {
    .superclass = &LT_Integer_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "SmallInteger",
    .instance_size = sizeof(LT_SmallInteger),
    .class_flags = LT_CLASS_FLAG_SPECIAL | LT_CLASS_FLAG_IMMUTABLE
        | LT_CLASS_FLAG_SCALAR,
    .debugPrintOn = SmallInteger_debugPrintOn,
};
