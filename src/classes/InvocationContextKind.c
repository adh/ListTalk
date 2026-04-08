/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/InvocationContextKind.h>

#include <ListTalk/vm/Class.h>

#include <inttypes.h>

static void InvocationContextKind_debugPrintOn(LT_Value obj, FILE* stream){
    fprintf(
        stream,
        "#<InvocationContextKind at 0x%" PRIxPTR ">",
        (uintptr_t)LT_VALUE_POINTER_VALUE(obj)
    );
}

LT_DEFINE_CLASS(LT_InvocationContextKind) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "InvocationContextKind",
    .instance_size = sizeof(LT_InvocationContextKind),
    .class_flags = LT_CLASS_FLAG_FINAL,
    .debugPrintOn = InvocationContextKind_debugPrintOn,
};
