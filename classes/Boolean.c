/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Boolean.h>
#include <ListTalk/vm/Class.h>

static void Boolean_debugPrintOn(LT_Value obj, FILE* stream){
    if (obj == LT_TRUE){
        fputs("true", stream);
        return;
    }
    if (obj == LT_FALSE){
        fputs("false", stream);
        return;
    }
    fputs("<boolean>", stream);
}

LT_DEFINE_CLASS(LT_Boolean) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .instance_size = sizeof(LT_Object),
    .class_flags = LT_CLASS_FLAG_SPECIAL | LT_CLASS_FLAG_IMMUTABLE
        | LT_CLASS_FLAG_SCALAR,
    .debugPrintOn = Boolean_debugPrintOn,
};
