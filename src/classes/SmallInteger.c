/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/vm/Class.h>

#include <inttypes.h>

struct LT_SmallInteger_s {
    LT_Object base;
};

static void SmallInteger_debugPrintOn(LT_Value obj, FILE* stream){
    if (LT_Value_is_fixnum(obj)){
        fprintf(stream, "%" PRId64, LT_SmallInteger_value(obj));
        return;
    }

    fprintf(stream, "#<small-integer 0x%" PRIxPTR ">", (uintptr_t)obj);
}

LT_DEFINE_CLASS(LT_SmallInteger) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "SmallInteger",
    .instance_size = sizeof(LT_SmallInteger),
    .class_flags = LT_CLASS_FLAG_SPECIAL | LT_CLASS_FLAG_IMMUTABLE
        | LT_CLASS_FLAG_SCALAR,
    .debugPrintOn = SmallInteger_debugPrintOn,
};
