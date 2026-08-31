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
#include <ListTalk/classes/Class.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/utils/utf8.h>
#include "src/utils/unicode_data.h"

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

#define CHARACTER_CASE_METHOD(name, selector, function, description) \
    LT_DEFINE_PRIMITIVE_FLAGS( \
        name, "Character>>" selector, "(self)", description, \
        LT_PRIMITIVE_FLAG_PURE \
    ){ \
        LT_Value cursor = arguments; \
        LT_Value self; \
        (void)tail_call_unwind_marker; \
        LT_OBJECT_ARG(cursor, self); \
        LT_ARG_END(cursor); \
        return LT_Character_new(function(LT_Character_value(self))); \
    }

CHARACTER_CASE_METHOD(character_method_lower_case, "lowerCase",
    LT_unicode_lowercase, "Return the Unicode simple lowercase mapping.")
CHARACTER_CASE_METHOD(character_method_upper_case, "upperCase",
    LT_unicode_uppercase, "Return the Unicode simple uppercase mapping.")
CHARACTER_CASE_METHOD(character_method_title_case, "titleCase",
    LT_unicode_titlecase, "Return the Unicode simple titlecase mapping.")

LT_DEFINE_PRIMITIVE_FLAGS(
    character_method_category,
    "Character>>category",
    "(self)",
    "Return the two-letter Unicode general category.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_String_new_cstr(
        (char*)LT_unicode_category(LT_Character_value(self))
    );
}

static int character_alphabetic_p(uint32_t codepoint){
    return LT_unicode_category(codepoint)[0] == 'L';
}
static int character_numeric_p(uint32_t codepoint){
    return LT_unicode_category(codepoint)[0] == 'N';
}
static int character_whitespace_p(uint32_t codepoint){
    return codepoint == 9 || codepoint == 10 || codepoint == 13
        || LT_unicode_category(codepoint)[0] == 'Z';
}
static int character_decimal_p(uint32_t codepoint){
    const char* category = LT_unicode_category(codepoint);
    return category[0] == 'N' && category[1] == 'd';
}
static int character_upper_case_p(uint32_t codepoint){
    const char* category = LT_unicode_category(codepoint);
    return category[0] == 'L' && category[1] == 'u';
}
static int character_lower_case_p(uint32_t codepoint){
    const char* category = LT_unicode_category(codepoint);
    return category[0] == 'L' && category[1] == 'l';
}
static int character_mark_p(uint32_t codepoint){
    return LT_unicode_category(codepoint)[0] == 'M';
}

#define CHARACTER_PROPERTY_METHOD(name, selector, predicate, description) \
    LT_DEFINE_PRIMITIVE_FLAGS( \
        name, "Character>>" selector, "(self)", description, \
        LT_PRIMITIVE_FLAG_PURE \
    ){ \
        LT_Value cursor = arguments; \
        LT_Value self; \
        (void)tail_call_unwind_marker; \
        LT_OBJECT_ARG(cursor, self); \
        LT_ARG_END(cursor); \
        return predicate(LT_Character_value(self)) ? LT_TRUE : LT_FALSE; \
    }

CHARACTER_PROPERTY_METHOD(character_method_alphabetic_p, "alphabetic?",
    character_alphabetic_p, "Return true for a Unicode letter.")
CHARACTER_PROPERTY_METHOD(character_method_numeric_p, "numeric?",
    character_numeric_p, "Return true for a Unicode number.")
CHARACTER_PROPERTY_METHOD(character_method_whitespace_p, "whitespace?",
    character_whitespace_p, "Return true for Unicode whitespace.")
CHARACTER_PROPERTY_METHOD(character_method_decimal_p, "decimal?",
    character_decimal_p, "Return true for a Unicode decimal digit.")
CHARACTER_PROPERTY_METHOD(character_method_upper_case_p, "upperCase?",
    character_upper_case_p, "Return true for an uppercase Unicode letter.")
CHARACTER_PROPERTY_METHOD(character_method_lower_case_p, "lowerCase?",
    character_lower_case_p, "Return true for a lowercase Unicode letter.")
CHARACTER_PROPERTY_METHOD(character_method_mark_p, "mark?",
    character_mark_p, "Return true for a Unicode combining mark.")

static LT_Method_Descriptor Character_methods[] = {
    {"asString", &character_method_as_string},
    {"lowerCase", &character_method_lower_case},
    {"upperCase", &character_method_upper_case},
    {"titleCase", &character_method_title_case},
    {"category", &character_method_category},
    {"alphabetic?", &character_method_alphabetic_p},
    {"numeric?", &character_method_numeric_p},
    {"whitespace?", &character_method_whitespace_p},
    {"decimal?", &character_method_decimal_p},
    {"upperCase?", &character_method_upper_case_p},
    {"lowerCase?", &character_method_lower_case_p},
    {"mark?", &character_method_mark_p},
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
