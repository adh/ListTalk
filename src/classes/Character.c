/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include <ListTalk/classes/Character.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/utils/utf8.h>

#include <ctype.h>
#include <inttypes.h>
#include <wchar.h>

struct LT_Character_s {
    LT_Object base;
};

static void Character_debugPrintOn(LT_Value obj, FILE* stream){
    uint32_t value = LT_Character_value(obj);

    fputs("#\\", stream);
    switch (value){
        case '\n':
            fputs("newline", stream);
            return;
        case '\r':
            fputs("return", stream);
            return;
        case '\t':
            fputs("tab", stream);
            return;
        case ' ':
            fputs("space", stream);
            return;
        default:
            if (value <= UINT32_C(0x7f) && isprint((int)value)){
                fputc((int)value, stream);
                return;
            }
            fprintf(stream, "u+%04" PRIx32, value);
            return;
    }
}

LT_DEFINE_PRIMITIVE(
    character_method_as_string,
    "Character>>asString",
    "(self)",
    "Return receiver encoded as a one-character UTF-8 string."
){
    LT_Value cursor = arguments;
    LT_Value self;
    char buffer[4];
    size_t length;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    length = LT_utf8_encode(LT_Character_value(self), buffer);
    if (length == 0){
        LT_error("Unable to encode character");
    }
    return (LT_Value)(uintptr_t)LT_String_new(buffer, length);
}

LT_DEFINE_PRIMITIVE(
    character_method_width,
    "Character>>width",
    "(self)",
    "Return display width measured with wcwidth."
){
    LT_Value cursor = arguments;
    LT_Value self;
    int width;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    width = wcwidth((wchar_t)LT_Character_value(self));
    return LT_SmallInteger_new((int64_t)width);
}

static LT_Method_Descriptor Character_methods[] = {
    {"asString", &character_method_as_string},
    {"width", &character_method_width},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Character) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Character",
    .documentation = "Unicode scalar character value.",
    .instance_size = sizeof(LT_Character),
    .class_flags = LT_CLASS_FLAG_SPECIAL | LT_CLASS_FLAG_IMMUTABLE
        | LT_CLASS_FLAG_SCALAR,
    .debugPrintOn = Character_debugPrintOn,
    .methods = Character_methods,
};
