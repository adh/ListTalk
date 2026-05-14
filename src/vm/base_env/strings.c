/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Character.h>
#include <ListTalk/classes/Symbol.h>
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
    return LT_Number_smallinteger_from_size(
        length,
        "String length does not fit fixnum"
    );
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

static void format_append_string(LT_StringBuilder* builder, LT_Value value){
    LT_String* string = LT_String_from_value(value);

    LT_StringBuilder_append_bytes(
        builder,
        LT_String_value_cstr(string),
        LT_String_byte_length(string)
    );
}

static LT_Value format_next_argument(LT_Value* cursor){
    LT_Value value;

    LT_OBJECT_ARG(*cursor, value);
    return value;
}

LT_DEFINE_PRIMITIVE(
    primitive_format,
    "format",
    "(format-string argument ...)",
    "Return a formatted string using SRFI-28-style directives."
){
    LT_Value cursor = arguments;
    LT_String* format_string;
    LT_StringBuilder* builder;
    const char* text;
    const char* end;

    LT_GENERIC_ARG(cursor, format_string, LT_String*, LT_String_from_value);

    builder = LT_StringBuilder_new();
    text = LT_String_value_cstr(format_string);
    end = text + LT_String_byte_length(format_string);

    while (text < end){
        char ch = *text++;

        if (ch != '~'){
            LT_StringBuilder_append_char(builder, ch);
            continue;
        }

        if (text == end){
            LT_error("Incomplete format directive");
        }

        ch = *text++;
        switch (ch){
            case 'a':
                format_append_string(
                    builder,
                    LT_send(
                        format_next_argument(&cursor),
                        LT_Symbol_new_in(LT_PACKAGE_KEYWORD, "asString"),
                        LT_NIL,
                        NULL
                    )
                );
                break;
            case 's':
                format_append_string(
                    builder,
                    (LT_Value)(uintptr_t)LT_Value_asString(
                        format_next_argument(&cursor)
                    )
                );
                break;
            case '%':
                LT_StringBuilder_append_char(builder, '\n');
                break;
            case '~':
                LT_StringBuilder_append_char(builder, '~');
                break;
            default:
                LT_error("Unknown format directive");
        }
    }

    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_String_new(
        LT_StringBuilder_value(builder),
        LT_StringBuilder_length(builder)
    );
}

LT_DEFINE_PRIMITIVE(
    primitive_string_join,
    "string-join",
    "(delimiter strings)",
    "Join a list of strings with delimiter."
){
    LT_Value cursor = arguments;
    LT_String* delimiter;
    LT_Value strings;

    LT_GENERIC_ARG(cursor, delimiter, LT_String*, LT_String_from_value);
    LT_OBJECT_ARG(cursor, strings);
    LT_ARG_END(cursor);

    return (LT_Value)(uintptr_t)LT_String_join(delimiter, strings);
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
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);

    if (!LT_Value_is_instance_of(value, LT_STATIC_CLASS(LT_Number))){
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
    LT_base_env_bind_static_primitive(environment, &primitive_format);
    LT_base_env_bind_static_primitive(environment, &primitive_string_join);
    LT_base_env_bind_static_primitive(environment, &primitive_substring);
    LT_base_env_bind_static_primitive(environment, &primitive_number_to_string);
}
