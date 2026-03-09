/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/classes/String.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/error.h>

static size_t checked_index_from_fixnum(int64_t value, const char* primitive_name){
    if (value < 0){
        LT_error("Negative index");
    }
    if (!LT_SmallInteger_in_range((int64_t)(size_t)value)){
        (void)primitive_name;
        LT_error("Index out of supported range");
    }
    return (size_t)value;
}

LT_DEFINE_PRIMITIVE(
    primitive_string_p,
    "string?",
    "(value)",
    "Return true when value is a string."
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_String_p(value) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    primitive_string_length,
    "string-length",
    "(string)",
    "Return string length as fixnum."
){
    LT_Value cursor = arguments;
    LT_String* string;
    size_t length;

    LT_GENERIC_ARG(cursor, string, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);

    length = LT_String_length(string);
    if (!LT_SmallInteger_in_range((int64_t)length)){
        LT_error("String length does not fit fixnum");
    }
    return LT_SmallInteger_new((int64_t)length);
}

LT_DEFINE_PRIMITIVE(
    primitive_string_ref,
    "string-ref",
    "(string index)",
    "Return byte value at index."
){
    LT_Value cursor = arguments;
    LT_String* string;
    int64_t index_value;
    unsigned char ch;

    LT_GENERIC_ARG(cursor, string, LT_String*, LT_String_from_value);
    LT_FIXNUM_ARG(cursor, index_value);
    LT_ARG_END(cursor);

    ch = LT_String_at(
        string,
        checked_index_from_fixnum(index_value, "string-ref")
    );
    return LT_SmallInteger_new((int64_t)ch);
}

LT_DEFINE_PRIMITIVE(
    primitive_string_append,
    "string-append",
    "(string ...)",
    "Concatenate all string arguments."
){
    LT_StringBuilder* builder = LT_StringBuilder_new();
    LT_Value cursor = arguments;

    while (cursor != LT_NIL){
        LT_String* string;
        size_t i;
        size_t length;
        const char* cstr;

        LT_GENERIC_ARG(cursor, string, LT_String*, LT_String_from_value);
        length = LT_String_length(string);
        cstr = LT_String_value_cstr(string);
        for (i = 0; i < length; i++){
            LT_StringBuilder_append_char(builder, cstr[i]);
        }
    }

    return (LT_Value)(uintptr_t)LT_String_new(
        LT_StringBuilder_value(builder),
        LT_StringBuilder_length(builder)
    );
}

void LT_base_env_bind_strings(LT_Environment* environment){
    LT_base_env_bind_static_primitive(environment, &primitive_string_p);
    LT_base_env_bind_static_primitive(environment, &primitive_string_length);
    LT_base_env_bind_static_primitive(environment, &primitive_string_ref);
    LT_base_env_bind_static_primitive(environment, &primitive_string_append);
}
