/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Nil.h>
#include <ListTalk/vm/Class.h>

static void Nil_debugPrintOn(LT_Value obj, FILE* stream){
    (void)obj;
    fputs("()", stream);
}

LT_DEFINE_CLASS(LT_Nil) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Nil",
    .instance_size = sizeof(LT_Object),
    .class_flags = LT_CLASS_FLAG_SPECIAL | LT_CLASS_FLAG_IMMUTABLE,
    .debugPrintOn = Nil_debugPrintOn,
};
