/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Character.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/value.h>

LT_DEFINE_PRIMITIVE(
    primitive_character_p,
    "character?",
    "(value)",
    "Return true when value is a character."
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_Character_p(value) ? LT_TRUE : LT_FALSE;
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
    "Return string length in Unicode code points as fixnum."
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
    "Return Unicode character at index."
){
    LT_Value cursor = arguments;
    LT_String* string;
    int64_t index_value;
    uint32_t ch;

    LT_GENERIC_ARG(cursor, string, LT_String*, LT_String_from_value);
    LT_FIXNUM_ARG(cursor, index_value);
    LT_ARG_END(cursor);

    ch = LT_String_at(
        string,
        checked_nonnegative_from_fixnum(index_value)
    );
    return LT_Character_new(ch);
}

LT_DEFINE_PRIMITIVE(
    primitive_string_to_character_list,
    "string->list",
    "(string)",
    "Return a list of characters in string order."
){
    LT_Value cursor = arguments;
    LT_String* string;

    LT_GENERIC_ARG(cursor, string, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);

    return LT_String_to_character_list(string);
}

LT_DEFINE_PRIMITIVE(
    primitive_character_list_to_string,
    "list->string",
    "(characters)",
    "Construct a string from a proper list of characters."
){
    LT_Value cursor = arguments;
    LT_Value characters;

    LT_OBJECT_ARG(cursor, characters);
    LT_ARG_END(cursor);

    return (LT_Value)(uintptr_t)LT_String_from_character_list(characters);
}

LT_DEFINE_PRIMITIVE(
    primitive_string_append,
    "string-append",
    "(string ...)",
    "Concatenate all string arguments."
){
    LT_Value cursor = arguments;
    LT_String* result = LT_String_new_cstr("");

    while (cursor != LT_NIL){
        LT_String* string;

        LT_GENERIC_ARG(cursor, string, LT_String*, LT_String_from_value);
        result = LT_String_append(result, string);
    }

    return (LT_Value)(uintptr_t)result;
}

LT_DEFINE_PRIMITIVE(
    primitive_substring,
    "substring",
    "(string from to)",
    "Return a substring using half-open Unicode code point indexes."
){
    LT_Value cursor = arguments;
    LT_String* string;
    int64_t from;
    int64_t to;

    LT_GENERIC_ARG(cursor, string, LT_String*, LT_String_from_value);
    LT_FIXNUM_ARG(cursor, from);
    LT_FIXNUM_ARG(cursor, to);
    LT_ARG_END(cursor);

    return (LT_Value)(uintptr_t)LT_String_substring(
        string,
        checked_nonnegative_from_fixnum(from),
        checked_nonnegative_from_fixnum(to)
    );
}

LT_DEFINE_PRIMITIVE(
    primitive_number_to_string,
    "number->string",
    "(number)",
    "Return the string representation of a number."
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);

    if (!LT_Number_value_p(value)){
        LT_type_error(value, &LT_Number_class);
    }

    return (LT_Value)(uintptr_t)LT_String_new_cstr(LT_Number_to_string(value));
}

void LT_base_env_bind_strings(LT_Environment* environment){
    LT_base_env_bind_static_primitive(environment, &primitive_character_p);
    LT_base_env_bind_static_primitive(environment, &primitive_string_p);
    LT_base_env_bind_static_primitive(environment, &primitive_string_length);
    LT_base_env_bind_static_primitive(environment, &primitive_string_ref);
    LT_base_env_bind_static_primitive(
        environment,
        &primitive_string_to_character_list
    );
    LT_base_env_bind_static_primitive(
        environment,
        &primitive_character_list_to_string
    );
    LT_base_env_bind_static_primitive(environment, &primitive_string_append);
    LT_base_env_bind_static_primitive(environment, &primitive_substring);
    LT_base_env_bind_static_primitive(environment, &primitive_number_to_string);
}
