/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/SourceLocation.h>
#include <ListTalk/macros/decl_macros.h>

struct LT_SourceLocation_s {
    LT_Object base;
};

static void SourceLocation_debugPrintOn(LT_Value obj, FILE* stream){
    fprintf(
        stream,
        "#<SourceLocation line=%u column=%u>",
        LT_SourceLocation_line(obj),
        LT_SourceLocation_column(obj)
    );
}

LT_DEFINE_CLASS(LT_SourceLocation) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "SourceLocation",
    .instance_size = sizeof(LT_SourceLocation),
    .class_flags = LT_CLASS_FLAG_SPECIAL | LT_CLASS_FLAG_FINAL
        | LT_CLASS_FLAG_IMMUTABLE | LT_CLASS_FLAG_SCALAR,
    .debugPrintOn = SourceLocation_debugPrintOn,
};
