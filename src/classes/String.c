/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "BigInteger_internal.h"

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/Dictionary.h>
#include <ListTalk/classes/Integer.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Character.h>
#include <ListTalk/classes/IdentityDictionary.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/RealNumber.h>
#include <ListTalk/classes/Set.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/utils.h>
#include <ListTalk/utils/utf8.h>

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

struct LT_String_s {
    LT_Object base;
    size_t length;
    size_t byte_length;
    char str[];
};

static void StringBuilder_append_codepoint(LT_StringBuilder* builder,
                                           uint32_t codepoint){
    char buffer[4];
    size_t length;

    length = LT_utf8_encode(codepoint, buffer);
    if (length == 0){
        length = LT_utf8_encode(LT_UTF8_REPLACEMENT_CODEPOINT, buffer);
    }
    LT_StringBuilder_append_bytes(builder, buffer, length);
}

static LT_String* String_new_normalized(const char* buf,
                                        size_t byte_length,
                                        size_t codepoint_length){
    LT_String* str = LT_Class_ALLOC_FLEXIBLE(LT_String, byte_length + 1);

    str->length = codepoint_length;
    str->byte_length = byte_length;
    if (byte_length > 0){
        memcpy(str->str, buf, byte_length);
    }
    str->str[byte_length] = '\0';
    return str;
}

static size_t String_byte_offset_for_codepoint_index(LT_String* string,
                                                     size_t index){
    const char* cursor = string->str;
    const char* end = string->str + string->byte_length;
    size_t codepoint_index = 0;

    if (index > string->length){
        LT_error("String index out of bounds");
    }

    while (codepoint_index < index && cursor < end){
        cursor = LT_String_utf8_next(cursor);
        codepoint_index++;
    }

    return (size_t)(cursor - string->str);
}

static const char* String_find_bytes(const char* haystack,
                                     size_t haystack_length,
                                     const char* needle,
                                     size_t needle_length){
    size_t index;

    if (needle_length == 0 || haystack_length < needle_length){
        return NULL;
    }

    for (index = 0; index <= haystack_length - needle_length; index++){
        if (memcmp(haystack + index, needle, needle_length) == 0){
            return haystack + index;
        }
    }

    return NULL;
}

static void String_error_if_empty_needle(LT_String* needle,
                                         const char* message){
    if (LT_String_byte_length(needle) == 0){
        LT_error(message);
    }
}

static size_t String_codepoint_index_at_cursor(LT_String* string,
                                               const char* target){
    const char* cursor = LT_String_value_cstr(string);
    size_t codepoint_index = 0;

    while (cursor < target){
        cursor = LT_String_utf8_next(cursor);
        codepoint_index++;
    }

    return codepoint_index;
}

static LT_String* String_replace_impl(LT_String* string,
                                      LT_String* needle,
                                      LT_String* replacement,
                                      int first_only){
    LT_StringBuilder* builder;
    const char* cursor;
    const char* end;
    const char* needle_bytes;
    size_t needle_byte_length;
    size_t codepoint_length = 0;

    String_error_if_empty_needle(
        needle,
        "String replacement pattern must not be empty"
    );

    builder = LT_StringBuilder_new();
    cursor = LT_String_value_cstr(string);
    end = cursor + LT_String_byte_length(string);
    needle_bytes = LT_String_value_cstr(needle);
    needle_byte_length = LT_String_byte_length(needle);

    while (cursor < end){
        const char* match = String_find_bytes(
            cursor,
            (size_t)(end - cursor),
            needle_bytes,
            needle_byte_length
        );

        if (match == NULL){
            while (cursor < end){
                const char* next = LT_String_utf8_next(cursor);
                LT_StringBuilder_append_bytes(
                    builder,
                    cursor,
                    (size_t)(next - cursor)
                );
                codepoint_length++;
                cursor = next;
            }
            break;
        }

        while (cursor < match){
            const char* next = LT_String_utf8_next(cursor);
            LT_StringBuilder_append_bytes(
                builder,
                cursor,
                (size_t)(next - cursor)
            );
            codepoint_length++;
            cursor = next;
        }

        LT_StringBuilder_append_bytes(
            builder,
            LT_String_value_cstr(replacement),
            LT_String_byte_length(replacement)
        );
        codepoint_length += LT_String_length(replacement);
        cursor += needle_byte_length;

        if (first_only){
            while (cursor < end){
                const char* next = LT_String_utf8_next(cursor);
                LT_StringBuilder_append_bytes(
                    builder,
                    cursor,
                    (size_t)(next - cursor)
                );
                codepoint_length++;
                cursor = next;
            }
            break;
        }
    }

    return String_new_normalized(
        LT_StringBuilder_value(builder),
        LT_StringBuilder_length(builder),
        codepoint_length
    );
}

static LT_String* String_from_span(const char* start,
                                   size_t byte_length,
                                   size_t codepoint_length){
    return String_new_normalized(start, byte_length, codepoint_length);
}

static int String_codepoint_is_ascii_space(uint32_t codepoint){
    return codepoint <= (uint32_t)UCHAR_MAX
        && isspace((int)codepoint);
}

static size_t String_count_codepoints_in_span(const char* start,
                                              const char* end){
    const char* cursor = start;
    size_t count = 0;

    while (cursor < end){
        cursor = LT_String_utf8_next(cursor);
        count++;
    }
    return count;
}

static int String_contains_codepoint(LT_String* string, uint32_t codepoint){
    const char* cursor = LT_String_value_cstr(string);
    const char* end = cursor + LT_String_byte_length(string);

    while (cursor < end){
        if (LT_String_utf8_codepoint_at(cursor) == codepoint){
            return 1;
        }
        cursor = LT_String_utf8_next(cursor);
    }
    return 0;
}

static int String_splitOn_delimiter_p(LT_Value delimiters, uint32_t codepoint){
    if (LT_Character_p(delimiters)){
        return LT_Character_value(delimiters) == codepoint;
    }

    if (LT_String_p(delimiters)){
        return String_contains_codepoint(
            LT_String_from_value(delimiters),
            codepoint
        );
    }

    if (LT_Value_is_instance_of(
        delimiters,
        (LT_Value)(uintptr_t)&LT_Set_class
    )){
        return LT_Set_contains(
            (LT_Set*)LT_VALUE_POINTER_VALUE(delimiters),
            LT_Character_new(codepoint)
        );
    }

    LT_error("splitOn: expects Character, String, or Set");
    return 0;
}

static void String_list_callback(LT_String* substring, void* baton){
    LT_ListBuilder_append((LT_ListBuilder*)baton, (LT_Value)(uintptr_t)substring);
}

static void String_callable_callback(LT_String* substring, void* baton){
    LT_Value callable = *(LT_Value*)baton;

    (void)LT_apply(
        callable,
        LT_cons((LT_Value)(uintptr_t)substring, LT_NIL),
        LT_NIL,
        LT_NIL,
        NULL
    );
}

static int String_dictionary_at(LT_Value dictionary,
                                LT_Value key,
                                LT_Value* value_out){
    if (LT_Value_is_instance_of(
        dictionary,
        (LT_Value)(uintptr_t)&LT_IdentityDictionary_class
    )){
        return LT_IdentityDictionary_at(
            (LT_IdentityDictionary*)LT_VALUE_POINTER_VALUE(dictionary),
            key,
            value_out
        );
    }

    if (LT_Dictionary_p(dictionary)){
        return LT_Dictionary_at(
            (LT_Dictionary*)LT_VALUE_POINTER_VALUE(dictionary),
            key,
            value_out
        );
    }

    LT_error("Expected Dictionary or IdentityDictionary");
    return 0;
}

LT_String* LT_String_mapCharacters(LT_String* string, LT_Value dictionary){
    LT_StringBuilder* builder = LT_StringBuilder_new();
    const char* cursor = LT_String_value_cstr(string);
    const char* end = cursor + LT_String_byte_length(string);
    size_t codepoint_length = 0;

    while (cursor < end){
        const char* next = LT_String_utf8_next(cursor);
        LT_Value character = LT_Character_new(LT_String_utf8_codepoint_at(cursor));
        LT_Value mapped;

        if (String_dictionary_at(dictionary, character, &mapped)){
            if (LT_Character_p(mapped)){
                StringBuilder_append_codepoint(builder, LT_Character_value(mapped));
                codepoint_length++;
            } else if (LT_String_p(mapped)){
                LT_String* mapped_string = LT_String_from_value(mapped);

                LT_StringBuilder_append_bytes(
                    builder,
                    LT_String_value_cstr(mapped_string),
                    LT_String_byte_length(mapped_string)
                );
                codepoint_length += LT_String_length(mapped_string);
            } else {
                LT_error("Character mapping values must be Character or String");
            }
        } else {
            LT_StringBuilder_append_bytes(
                builder,
                cursor,
                (size_t)(next - cursor)
            );
            codepoint_length++;
        }

        cursor = next;
    }

    return String_new_normalized(
        LT_StringBuilder_value(builder),
        LT_StringBuilder_length(builder),
        codepoint_length
    );
}

void LT_String_substringsDoWhitespace(LT_String* string,
                                      LT_String_SubstringCallback callback,
                                      void* baton){
    const char* cursor = LT_String_value_cstr(string);
    const char* end = cursor + LT_String_byte_length(string);

    while (cursor < end){
        const char* start;
        size_t codepoint_length = 0;

        while (cursor < end
            && String_codepoint_is_ascii_space(LT_String_utf8_codepoint_at(cursor))){
            cursor = LT_String_utf8_next(cursor);
        }

        start = cursor;
        while (cursor < end
            && !String_codepoint_is_ascii_space(LT_String_utf8_codepoint_at(cursor))){
            cursor = LT_String_utf8_next(cursor);
            codepoint_length++;
        }

        if (start < cursor){
            callback(
                String_from_span(start, (size_t)(cursor - start), codepoint_length),
                baton
            );
        }
    }
}

LT_Value LT_String_substringsWhitespace(LT_String* string){
    LT_ListBuilder* builder = LT_ListBuilder_new();

    LT_String_substringsDoWhitespace(string, String_list_callback, builder);
    return LT_ListBuilder_value(builder);
}

void LT_String_substringsDo(LT_String* string,
                            LT_String* delimiter,
                            LT_String_SubstringCallback callback,
                            void* baton){
    const char* cursor = LT_String_value_cstr(string);
    const char* end = cursor + LT_String_byte_length(string);
    const char* delimiter_bytes = LT_String_value_cstr(delimiter);
    size_t delimiter_byte_length = LT_String_byte_length(delimiter);

    if (delimiter_byte_length == 0){
        LT_error("String delimiter must not be empty");
    }

    while (1){
        const char* match = String_find_bytes(
            cursor,
            (size_t)(end - cursor),
            delimiter_bytes,
            delimiter_byte_length
        );
        const char* segment_end = match == NULL ? end : match;

        callback(
            String_from_span(
                cursor,
                (size_t)(segment_end - cursor),
                String_count_codepoints_in_span(cursor, segment_end)
            ),
            baton
        );

        if (match == NULL){
            break;
        }
        cursor = match + delimiter_byte_length;
    }
}

LT_Value LT_String_substrings(LT_String* string, LT_String* delimiter){
    LT_ListBuilder* builder = LT_ListBuilder_new();

    LT_String_substringsDo(string, delimiter, String_list_callback, builder);
    return LT_ListBuilder_value(builder);
}

void LT_String_splitOnDo(LT_String* string,
                         LT_Value delimiters,
                         LT_String_SubstringCallback callback,
                         void* baton){
    const char* cursor = LT_String_value_cstr(string);
    const char* end = cursor + LT_String_byte_length(string);
    const char* start = cursor;
    size_t codepoint_length = 0;

    while (cursor < end){
        uint32_t codepoint = LT_String_utf8_codepoint_at(cursor);
        const char* next = LT_String_utf8_next(cursor);

        if (String_splitOn_delimiter_p(delimiters, codepoint)){
            callback(
                String_from_span(start, (size_t)(cursor - start), codepoint_length),
                baton
            );
            start = next;
            codepoint_length = 0;
        } else {
            codepoint_length++;
        }
        cursor = next;
    }

    callback(
        String_from_span(start, (size_t)(end - start), codepoint_length),
        baton
    );
}

LT_Value LT_String_splitOn(LT_String* string, LT_Value delimiters){
    LT_ListBuilder* builder = LT_ListBuilder_new();

    LT_String_splitOnDo(string, delimiters, String_list_callback, builder);
    return LT_ListBuilder_value(builder);
}

int LT_String_contains(LT_String* string, LT_String* needle){
    String_error_if_empty_needle(needle, "String search pattern must not be empty");
    return String_find_bytes(
        LT_String_value_cstr(string),
        LT_String_byte_length(string),
        LT_String_value_cstr(needle),
        LT_String_byte_length(needle)
    ) != NULL;
}

int LT_String_startsWith(LT_String* string, LT_String* prefix){
    size_t prefix_length = LT_String_byte_length(prefix);

    return prefix_length <= LT_String_byte_length(string)
        && memcmp(
            LT_String_value_cstr(string),
            LT_String_value_cstr(prefix),
            prefix_length
        ) == 0;
}

int LT_String_find(LT_String* string, LT_String* needle, size_t* index_out){
    const char* match;

    String_error_if_empty_needle(needle, "String search pattern must not be empty");
    match = String_find_bytes(
        LT_String_value_cstr(string),
        LT_String_byte_length(string),
        LT_String_value_cstr(needle),
        LT_String_byte_length(needle)
    );
    if (match == NULL){
        return 0;
    }

    if (index_out != NULL){
        *index_out = String_codepoint_index_at_cursor(string, match);
    }
    return 1;
}

LT_Value LT_String_findAll(LT_String* string, LT_String* needle){
    LT_ListBuilder* builder = LT_ListBuilder_new();
    const char* cursor = LT_String_value_cstr(string);
    const char* end = cursor + LT_String_byte_length(string);
    const char* needle_bytes;
    size_t needle_byte_length;
    size_t codepoint_index = 0;

    String_error_if_empty_needle(needle, "String search pattern must not be empty");
    needle_bytes = LT_String_value_cstr(needle);
    needle_byte_length = LT_String_byte_length(needle);

    while (cursor < end){
        const char* match = String_find_bytes(
            cursor,
            (size_t)(end - cursor),
            needle_bytes,
            needle_byte_length
        );

        if (match == NULL){
            break;
        }

        while (cursor < match){
            cursor = LT_String_utf8_next(cursor);
            codepoint_index++;
        }

        LT_ListBuilder_append(
            builder,
            LT_Number_smallinteger_from_size(
                codepoint_index,
                "String match index does not fit fixnum"
            )
        );

        cursor += needle_byte_length;
        codepoint_index += LT_String_length(needle);
    }

    return LT_ListBuilder_value(builder);
}

static size_t String_hash(LT_Value value){
    LT_String* string = LT_String_from_value(value);
    const unsigned char* cursor =
        (const unsigned char*)LT_String_value_cstr(string);
    size_t index;
    uint32_t hash = UINT32_C(0x811c9dc5);

    for (index = 0; index < LT_String_byte_length(string); index++){
        hash += (uint32_t)cursor[index];
        hash *= UINT32_C(0x01000193);
    }
    return (size_t)hash;
}

static int String_equal_p(LT_Value left, LT_Value right){
    LT_String* left_string;
    LT_String* right_string;
    size_t length;

    if (!LT_String_p(right)){
        return 0;
    }

    left_string = LT_String_from_value(left);
    right_string = LT_String_from_value(right);
    length = LT_String_byte_length(left_string);

    if (length != LT_String_byte_length(right_string)){
        return 0;
    }

    return memcmp(
        LT_String_value_cstr(left_string),
        LT_String_value_cstr(right_string),
        length
    ) == 0;
}

static void String_debugPrintOn(LT_Value obj, FILE* stream){
    LT_String* string = LT_String_from_value(obj);
    const char* cursor = LT_String_value_cstr(string);
    const char* end = cursor + LT_String_byte_length(string);

    fputc('"', stream);
    while (cursor < end){
        unsigned char ch = (unsigned char)(*cursor);
        switch (ch){
            case '\n':
                fputs("\\n", stream);
                break;
            case '\r':
                fputs("\\r", stream);
                break;
            case '\t':
                fputs("\\t", stream);
                break;
            case '\\':
                fputs("\\\\", stream);
                break;
            case '"':
                fputs("\\\"", stream);
                break;
            default:
                if (ch >= 0x80 || isprint(ch)){
                    fputc((int)ch, stream);
                } else {
                    fprintf(stream, "\\x%02x", (unsigned int)ch);
                }
                break;
        }
        cursor++;
    }
    fputc('"', stream);
}

LT_DEFINE_PRIMITIVE(
    string_method_length,
    "String>>length",
    "(self)",
    "Return string length in Unicode code points."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* string;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    string = LT_String_from_value(self);
    return LT_Number_smallinteger_from_size(
        LT_String_length(string),
        "String length does not fit fixnum"
    );
}

LT_DEFINE_PRIMITIVE(
    string_method_at,
    "String>>at:",
    "(self index)",
    "Return Unicode character at index."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value index;
    LT_String* string;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, index);
    LT_ARG_END(cursor);

    string = LT_String_from_value(self);
    return LT_Character_new((uint32_t)LT_String_at(
        string,
        LT_Number_nonnegative_size_from_integer(
            index,
            "String index out of bounds",
            "String index out of bounds"
        )
    ));
}

LT_DEFINE_PRIMITIVE(
    string_method_append,
    "String>>append:",
    "(self other)",
    "Return a new string with other appended."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* left;
    LT_String* right;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, right, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);

    left = LT_String_from_value(self);
    return (LT_Value)(uintptr_t)LT_String_append(left, right);
}

LT_DEFINE_PRIMITIVE(
    string_method_replace_with,
    "String>>replace:with:",
    "(self needle replacement)",
    "Return a new string replacing all occurrences of needle with replacement."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* needle;
    LT_String* replacement;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, needle, LT_String*, LT_String_from_value);
    LT_GENERIC_ARG(cursor, replacement, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);

    return (LT_Value)(uintptr_t)LT_String_replace(
        LT_String_from_value(self),
        needle,
        replacement
    );
}

LT_DEFINE_PRIMITIVE(
    string_method_replace_first_with,
    "String>>replaceFirst:with:",
    "(self needle replacement)",
    "Return a new string replacing the first occurrence of needle with replacement."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* needle;
    LT_String* replacement;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, needle, LT_String*, LT_String_from_value);
    LT_GENERIC_ARG(cursor, replacement, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);

    return (LT_Value)(uintptr_t)LT_String_replaceFirst(
        LT_String_from_value(self),
        needle,
        replacement
    );
}

LT_DEFINE_PRIMITIVE(
    string_method_map_characters,
    "String>>mapCharacters:",
    "(self dictionary)",
    "Return a new string mapping characters through a dictionary."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value dictionary;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, dictionary);
    LT_ARG_END(cursor);

    return (LT_Value)(uintptr_t)LT_String_mapCharacters(
        LT_String_from_value(self),
        dictionary
    );
}

LT_DEFINE_PRIMITIVE(
    string_method_substrings_whitespace,
    "String>>substrings",
    "(self)",
    "Return non-empty substrings split on whitespace."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    return LT_String_substringsWhitespace(LT_String_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    string_method_substrings_do_whitespace,
    "String>>substringsDo:",
    "(self callable)",
    "Call callable for each non-empty substring split on whitespace."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);

    LT_String_substringsDoWhitespace(
        LT_String_from_value(self),
        String_callable_callback,
        &callable
    );
    return LT_NIL;
}

LT_DEFINE_PRIMITIVE(
    string_method_substrings,
    "String>>substrings:",
    "(self delimiter)",
    "Return substrings split on delimiter string, including empty substrings."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* delimiter;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, delimiter, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);

    return LT_String_substrings(LT_String_from_value(self), delimiter);
}

LT_DEFINE_PRIMITIVE(
    string_method_substrings_do,
    "String>>substrings:do:",
    "(self delimiter callable)",
    "Call callable for each substring split on delimiter string."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* delimiter;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, delimiter, LT_String*, LT_String_from_value);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);

    LT_String_substringsDo(
        LT_String_from_value(self),
        delimiter,
        String_callable_callback,
        &callable
    );
    return LT_NIL;
}

LT_DEFINE_PRIMITIVE(
    string_method_split_on,
    "String>>splitOn:",
    "(self delimiters)",
    "Return substrings split on a delimiter character, delimiter string, or set of characters."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value delimiters;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, delimiters);
    LT_ARG_END(cursor);

    return LT_String_splitOn(LT_String_from_value(self), delimiters);
}

LT_DEFINE_PRIMITIVE(
    string_method_split_on_do,
    "String>>splitOn:do:",
    "(self delimiters callable)",
    "Call callable for substrings split on a delimiter character, delimiter string, or set of characters."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value delimiters;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, delimiters);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);

    LT_String_splitOnDo(
        LT_String_from_value(self),
        delimiters,
        String_callable_callback,
        &callable
    );
    return LT_NIL;
}

LT_DEFINE_PRIMITIVE(
    string_method_join,
    "String>>join:",
    "(self strings)",
    "Join a list of strings using self as the delimiter."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value strings;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, strings);
    LT_ARG_END(cursor);

    return (LT_Value)(uintptr_t)LT_String_join(
        LT_String_from_value(self),
        strings
    );
}

LT_DEFINE_PRIMITIVE(
    string_method_contains,
    "String>>contains?:",
    "(self needle)",
    "Return true when string contains needle."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* needle;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, needle, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);

    return LT_String_contains(LT_String_from_value(self), needle)
        ? LT_TRUE
        : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    string_method_starts_with,
    "String>>startsWith?:",
    "(self prefix)",
    "Return true when string starts with prefix."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* prefix;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, prefix, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);

    return LT_String_startsWith(LT_String_from_value(self), prefix)
        ? LT_TRUE
        : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    string_method_find,
    "String>>find:",
    "(self needle)",
    "Return first codepoint index of needle, or nil when absent."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* needle;
    size_t index;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, needle, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);

    if (!LT_String_find(LT_String_from_value(self), needle, &index)){
        return LT_NIL;
    }
    return LT_Number_smallinteger_from_size(
        index,
        "String match index does not fit fixnum"
    );
}

LT_DEFINE_PRIMITIVE(
    string_method_find_all,
    "String>>findAll:",
    "(self needle)",
    "Return all codepoint indices where needle occurs."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* needle;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, needle, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);

    return LT_String_findAll(LT_String_from_value(self), needle);
}

LT_DEFINE_PRIMITIVE(
    string_method_as_bytevector,
    "String>>asByteVector",
    "(self)",
    "Return a bytevector containing string UTF-8 bytes."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    return (LT_Value)(uintptr_t)LT_ByteVector_from_string(
        LT_String_from_value(self)
    );
}

LT_DEFINE_PRIMITIVE(
    string_method_as_string,
    "String>>asString",
    "(self)",
    "Return receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    if (!LT_String_p(self)){
        LT_type_error(self, &LT_String_class);
    }
    return self;
}

LT_DEFINE_PRIMITIVE(
    string_method_as_list,
    "String>>asList",
    "(self)",
    "Return string characters as a list."
){
    LT_Value cursor = arguments;
    LT_String* string;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, string, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);
    return LT_String_to_character_list(string);
}

LT_DEFINE_PRIMITIVE(
    string_method_substring_from_to,
    "String>>substringFrom:to:",
    "(self from to)",
    "Return a substring using half-open Unicode code point indexes."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value from;
    LT_Value to;
    LT_String* string;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, from);
    LT_OBJECT_ARG(cursor, to);
    LT_ARG_END(cursor);

    string = LT_String_from_value(self);
    return (LT_Value)(uintptr_t)LT_String_substring(
        string,
        LT_Number_nonnegative_size_from_integer(
            from,
            "String index out of bounds",
            "String index out of bounds"
        ),
        LT_Number_nonnegative_size_from_integer(
            to,
            "String index out of bounds",
            "String index out of bounds"
        )
    );
}

static LT_Method_Descriptor String_methods[] = {
    {"length", &string_method_length},
    {"at:", &string_method_at},
    {"append:", &string_method_append},
    {"replace:with:", &string_method_replace_with},
    {"replaceFirst:with:", &string_method_replace_first_with},
    {"mapCharacters:", &string_method_map_characters},
    {"substrings", &string_method_substrings_whitespace},
    {"substringsDo:", &string_method_substrings_do_whitespace},
    {"substrings:", &string_method_substrings},
    {"substrings:do:", &string_method_substrings_do},
    {"splitOn:", &string_method_split_on},
    {"splitOn:do:", &string_method_split_on_do},
    {"join:", &string_method_join},
    {"contains?:", &string_method_contains},
    {"startsWith?:", &string_method_starts_with},
    {"find:", &string_method_find},
    {"findAll:", &string_method_find_all},
    {"asByteVector", &string_method_as_bytevector},
    {"asString", &string_method_as_string},
    {"asList", &string_method_as_list},
    {"from:to:", &string_method_substring_from_to},
    {"substringFrom:to:", &string_method_substring_from_to},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_String) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "String",
    .instance_size = sizeof(LT_String),
    .hash = String_hash,
    .equal_p = String_equal_p,
    .debugPrintOn = String_debugPrintOn,
    .methods = String_methods,
};

LT_String* LT_String_new(char* buf, size_t len){
    LT_StringBuilder* builder = LT_StringBuilder_new();
    const char* cursor = buf;
    const char* end = buf + len;
    size_t codepoint_length = 0;
    size_t byte_length;

    while (cursor < end){
        uint32_t codepoint;
        const char* next;
        size_t available = (size_t)(end - cursor);

        codepoint = LT_utf8_codepoint_at_bounded(cursor, available);
        next = LT_utf8_next_bounded(cursor, available);

        StringBuilder_append_codepoint(builder, codepoint);
        codepoint_length++;
        cursor = next;
    }

    byte_length = LT_StringBuilder_length(builder);
    return String_new_normalized(
        LT_StringBuilder_value(builder),
        byte_length,
        codepoint_length
    );
}
LT_String* LT_String_new_cstr(char* str){
    return LT_String_new(str, strlen(str));
}

LT_String* LT_String_append(LT_String* left, LT_String* right){
    size_t byte_length =
        LT_String_byte_length(left) + LT_String_byte_length(right);
    size_t codepoint_length =
        LT_String_length(left) + LT_String_length(right);
    char* buffer = byte_length == 0 ? "" : GC_MALLOC_ATOMIC(byte_length);

    memcpy(buffer, LT_String_value_cstr(left), LT_String_byte_length(left));
    memcpy(
        buffer + LT_String_byte_length(left),
        LT_String_value_cstr(right),
        LT_String_byte_length(right)
    );
    return String_new_normalized(buffer, byte_length, codepoint_length);
}

LT_String* LT_String_replace(LT_String* string,
                             LT_String* needle,
                             LT_String* replacement){
    return String_replace_impl(string, needle, replacement, 0);
}

LT_String* LT_String_replaceFirst(LT_String* string,
                                  LT_String* needle,
                                  LT_String* replacement){
    return String_replace_impl(string, needle, replacement, 1);
}

LT_String* LT_String_join(LT_String* delimiter, LT_Value strings){
    LT_StringBuilder* builder = LT_StringBuilder_new();
    LT_Value cursor = strings;
    size_t codepoint_length = 0;
    int first = 1;

    while (cursor != LT_NIL){
        LT_String* string;

        if (!LT_Pair_p(cursor)){
            LT_error("string-join expects a proper list of strings");
        }

        string = LT_String_from_value(LT_car(cursor));
        if (!first){
            LT_StringBuilder_append_bytes(
                builder,
                LT_String_value_cstr(delimiter),
                LT_String_byte_length(delimiter)
            );
            codepoint_length += LT_String_length(delimiter);
        }

        LT_StringBuilder_append_bytes(
            builder,
            LT_String_value_cstr(string),
            LT_String_byte_length(string)
        );
        codepoint_length += LT_String_length(string);
        first = 0;
        cursor = LT_cdr(cursor);
    }

    return String_new_normalized(
        LT_StringBuilder_value(builder),
        LT_StringBuilder_length(builder),
        codepoint_length
    );
}

static void String_format_append_string(LT_StringBuilder* builder,
                                        LT_Value value){
    LT_String* string = LT_String_from_value(value);

    LT_StringBuilder_append_bytes(
        builder,
        LT_String_value_cstr(string),
        LT_String_byte_length(string)
    );
}

static char* String_format_decimal_cstr(LT_Value value){
    if (!LT_Value_is_instance_of(value, LT_STATIC_CLASS(LT_RealNumber))){
        LT_type_error(value, &LT_RealNumber_class);
    }

    if (LT_Integer_value_p(value)){
        return LT_Number_to_string(value);
    }

    return LT_sprintf("%.17g", LT_Number_to_double(value));
}

static char* String_format_float_cstr(LT_Value value, char directive){
    if (!LT_Value_is_instance_of(value, LT_STATIC_CLASS(LT_RealNumber))){
        LT_type_error(value, &LT_RealNumber_class);
    }

    switch (directive){
        case 'f':
            return LT_sprintf("%f", LT_Number_to_double(value));
        case 'e':
            return LT_sprintf("%e", LT_Number_to_double(value));
        case 'g':
            return LT_sprintf("%g", LT_Number_to_double(value));
        default:
            LT_error("Internal float format directive error");
    }
}

static char* String_format_integer_radix_cstr(LT_Value value,
                                              unsigned int radix){
    static const char digits[] = "0123456789abcdef";
    LT_StringBuilder* reversed = LT_StringBuilder_new();
    char* reversed_digits;
    char* result;
    size_t digit_count;
    size_t index;
    int negative;

    if (!LT_Integer_value_p(value)){
        LT_type_error(value, &LT_Integer_class);
    }

    negative = LT_Integer_negative_p(value);
    value = LT_Integer_abs(value);

    if (LT_Integer_is_zero(value)){
        return "0";
    }

    while (!LT_Integer_is_zero(value)){
        LT_Value quotient;
        LT_Value remainder;
        uint32_t digit;

        LT_Integer_divmod(
            value,
            LT_SmallInteger_new((int64_t)radix),
            &quotient,
            &remainder
        );
        if (!LT_Integer_to_uint32(remainder, &digit) || digit >= radix){
            LT_error("Internal integer radix conversion error");
        }
        LT_StringBuilder_append_char(reversed, digits[digit]);
        value = quotient;
    }

    reversed_digits = LT_StringBuilder_value(reversed);
    digit_count = LT_StringBuilder_length(reversed);
    result = GC_MALLOC_ATOMIC(digit_count + (negative ? 2 : 1));
    if (negative){
        result[0] = '-';
    }
    for (index = 0; index < digit_count; index++){
        result[(negative ? 1 : 0) + index] =
            reversed_digits[digit_count - index - 1];
    }
    result[digit_count + (negative ? 1 : 0)] = '\0';
    return result;
}

static void String_format_append_cstr(LT_StringBuilder* builder, char* text){
    LT_StringBuilder_append_str(builder, text);
}

static LT_Value String_format_next_argument(LT_Value* cursor){
    LT_Value value;

    LT_OBJECT_ARG(*cursor, value);
    return value;
}

static LT_Value String_format_as_list(LT_Value value){
    LT_Value list = LT_send(
        value,
        LT_Symbol_new_in(LT_PACKAGE_KEYWORD, "asList"),
        LT_NIL,
        NULL
    );

    if (!LT_List_p(list)){
        LT_error("Format iteration asList must return a list");
    }
    return list;
}

typedef struct String_FormatDirective_s {
    char directive;
    int colon;
    int atsign;
    int has_numeric_argument;
    size_t numeric_argument;
} String_FormatDirective;

static String_FormatDirective String_format_parse_directive(const char** text,
                                                            const char* end){
    String_FormatDirective directive = {0, 0, 0, 0, 0};
    int parsing = 1;

    while (parsing){
        if (*text == end){
            LT_error("Incomplete format directive");
        }

        if (isdigit((unsigned char)**text)){
            if (directive.has_numeric_argument){
                LT_error("Too many format directive numeric arguments");
            }

            directive.has_numeric_argument = 1;
            while (*text < end && isdigit((unsigned char)**text)){
                directive.numeric_argument =
                    directive.numeric_argument * 10
                    + (size_t)(*(*text)++ - '0');
            }
            continue;
        }

        switch (**text){
            case ':':
                directive.colon = 1;
                (*text)++;
                break;
            case '@':
                directive.atsign = 1;
                (*text)++;
                break;
            default:
                parsing = 0;
                break;
        }
    }

    if (*text == end){
        LT_error("Incomplete format directive");
    }

    directive.directive = *(*text)++;
    return directive;
}

static void String_format_into(LT_StringBuilder* builder,
                               const char* text,
                               const char* end,
                               LT_Value* cursor,
                               int allow_iteration_end);

static const char* String_format_find_iteration_end(const char* text,
                                                    const char* end,
                                                    const char** closing_end){
    size_t depth = 1;

    while (text < end){
        const char* directive_start;

        if (*text++ != '~'){
            continue;
        }

        if (text == end){
            LT_error("Incomplete format directive");
        }

        directive_start = text - 1;
        switch (String_format_parse_directive(&text, end).directive){
            case '{':
                depth++;
                break;
            case '}':
                depth--;
                if (depth == 0){
                    *closing_end = text;
                    return directive_start;
                }
                break;
            default:
                break;
        }
    }

    LT_error("Unterminated format iteration directive");
}

static void String_format_iteration_into(LT_StringBuilder* builder,
                                         const char* text,
                                         const char* iteration_end,
                                         LT_Value* cursor,
                                         String_FormatDirective directive){
    size_t iteration_count = 0;
    size_t iteration_limit = directive.has_numeric_argument
        ? directive.numeric_argument
        : (size_t)-1;

    if (directive.atsign){
        LT_Value iteration_cursor =
            String_format_as_list(String_format_next_argument(cursor));

        while (iteration_cursor != LT_NIL && iteration_count < iteration_limit){
            if (!LT_Pair_p(iteration_cursor)){
                LT_error("Format iteration expects a proper list");
            }

            if (directive.colon){
                LT_Value sub_cursor = String_format_as_list(
                    LT_car(iteration_cursor)
                );

                String_format_into(
                    builder,
                    text,
                    iteration_end,
                    &sub_cursor,
                    0
                );
                LT_ARG_END(sub_cursor);
            } else {
                LT_Value previous_cursor = iteration_cursor;

                String_format_into(
                    builder,
                    text,
                    iteration_end,
                    &iteration_cursor,
                    0
                );
                if (iteration_cursor == previous_cursor){
                    LT_error(
                        "Format iteration body must consume an argument"
                    );
                }
            }
            if (directive.colon){
                iteration_cursor = LT_cdr(iteration_cursor);
            }
            iteration_count++;
        }
        return;
    }

    if (directive.colon){
        LT_Value outer_cursor = String_format_next_argument(cursor);

        while (outer_cursor != LT_NIL && iteration_count < iteration_limit){
            LT_Value sub_cursor;

            if (!LT_Pair_p(outer_cursor)){
                LT_error("Format iteration expects a proper list");
            }

            sub_cursor = LT_car(outer_cursor);
            if (!LT_List_p(sub_cursor)){
                LT_error("Format iteration expects a proper list");
            }

            String_format_into(
                builder,
                text,
                iteration_end,
                &sub_cursor,
                0
            );
            LT_ARG_END(sub_cursor);
            outer_cursor = LT_cdr(outer_cursor);
            iteration_count++;
        }
        return;
    }

    {
        LT_Value iteration_cursor = String_format_next_argument(cursor);

        while (iteration_cursor != LT_NIL && iteration_count < iteration_limit){
            LT_Value previous_cursor = iteration_cursor;

            if (!LT_Pair_p(iteration_cursor)){
                LT_error("Format iteration expects a proper list");
            }

            String_format_into(
                builder,
                text,
                iteration_end,
                &iteration_cursor,
                0
            );
            if (iteration_cursor == previous_cursor){
                LT_error(
                    "Format iteration body must consume an argument"
                );
            }
            iteration_count++;
        }
    }
}

static void String_format_into(LT_StringBuilder* builder,
                               const char* text,
                               const char* end,
                               LT_Value* cursor,
                               int allow_iteration_end){

    while (text < end){
        char ch = *text++;

        if (ch != '~'){
            LT_StringBuilder_append_char(builder, ch);
            continue;
        }

        if (text == end){
            LT_error("Incomplete format directive");
        }

        {
            String_FormatDirective directive =
                String_format_parse_directive(&text, end);
            ch = directive.directive;
            switch (ch){
            case 'a':
                String_format_append_string(
                    builder,
                    LT_send(
                        String_format_next_argument(cursor),
                        LT_Symbol_new_in(LT_PACKAGE_KEYWORD, "asString"),
                        LT_NIL,
                        NULL
                    )
                );
                break;
            case 's':
                String_format_append_string(
                    builder,
                    (LT_Value)(uintptr_t)LT_Value_asString(
                        String_format_next_argument(cursor)
                    )
                );
                break;
            case 'd':
                String_format_append_cstr(
                    builder,
                    String_format_decimal_cstr(
                        String_format_next_argument(cursor)
                    )
                );
                break;
            case 'b':
                String_format_append_cstr(
                    builder,
                    String_format_integer_radix_cstr(
                        String_format_next_argument(cursor),
                        2
                    )
                );
                break;
            case 'o':
                String_format_append_cstr(
                    builder,
                    String_format_integer_radix_cstr(
                        String_format_next_argument(cursor),
                        8
                    )
                );
                break;
            case 'x':
                String_format_append_cstr(
                    builder,
                    String_format_integer_radix_cstr(
                        String_format_next_argument(cursor),
                        16
                    )
                );
                break;
            case 'f':
            case 'e':
            case 'g':
                String_format_append_cstr(
                    builder,
                    String_format_float_cstr(
                        String_format_next_argument(cursor),
                        ch
                    )
                );
                break;
            case '%':
                LT_StringBuilder_append_char(builder, '\n');
                break;
            case '~':
                LT_StringBuilder_append_char(builder, '~');
                break;
            case '{': {
                const char* closing_end;
                const char* iteration_end =
                    String_format_find_iteration_end(
                        text,
                        end,
                        &closing_end
                    );
                String_format_iteration_into(
                    builder,
                    text,
                    iteration_end,
                    cursor,
                    directive
                );
                text = closing_end;
                break;
            }
            case '}':
                if (allow_iteration_end){
                    return;
                }
                LT_error("Unmatched format iteration terminator");
            default:
                LT_error("Unknown format directive");
            }
        }
    }
}

LT_String* LT_String_format(LT_String* format_string, LT_Value arguments){
    LT_Value cursor = arguments;
    LT_StringBuilder* builder = LT_StringBuilder_new();
    const char* text = LT_String_value_cstr(format_string);
    const char* end = text + LT_String_byte_length(format_string);

    String_format_into(builder, text, end, &cursor, 0);
    LT_ARG_END(cursor);
    return LT_String_new(
        LT_StringBuilder_value(builder),
        LT_StringBuilder_length(builder)
    );
}

LT_String* LT_String_substring(LT_String* string, size_t from, size_t to){
    size_t from_byte;
    size_t to_byte;

    if (from > to || to > LT_String_length(string)){
        LT_error("String index out of bounds");
    }

    from_byte = String_byte_offset_for_codepoint_index(string, from);
    to_byte = String_byte_offset_for_codepoint_index(string, to);
    return String_new_normalized(
        LT_String_value_cstr(string) + from_byte,
        to_byte - from_byte,
        to - from
    );
}

const char* LT_String_value_cstr(LT_String* string){
    return string->str;
}

size_t LT_String_length(LT_String* string){
    return string->length;
}

size_t LT_String_byte_length(LT_String* string){
    return string->byte_length;
}

uint32_t LT_String_utf8_codepoint_at(const char* cursor){
    return LT_utf8_codepoint_at(cursor);
}

const char* LT_String_utf8_next(const char* cursor){
    return LT_utf8_next(cursor);
}

uint32_t LT_String_at(LT_String* string, size_t index){
    const char* cursor = string->str;
    const char* end = string->str + string->byte_length;
    size_t codepoint_index = 0;

    if (index >= string->length){
        LT_error("String index out of bounds");
    }

    while (cursor < end){
        if (codepoint_index == index){
            return LT_String_utf8_codepoint_at(cursor);
        }
        cursor = LT_String_utf8_next(cursor);
        codepoint_index++;
    }

    LT_error("String index out of bounds");
    return 0;
}

LT_Value LT_String_to_character_list(LT_String* string){
    LT_Value list = LT_NIL;
    const char* start = string->str;
    const char* cursor = start + string->byte_length;

    while (cursor > start){
        const char* previous = start;
        const char* next = start;

        while (next < cursor){
            previous = next;
            next = LT_String_utf8_next(next);
        }
        list = LT_cons(LT_Character_new(LT_String_utf8_codepoint_at(previous)),
                       list);
        cursor = previous;
    }
    return list;
}

LT_String* LT_String_from_character_list(LT_Value characters){
    LT_StringBuilder* builder = LT_StringBuilder_new();
    LT_Value cursor = characters;
    size_t codepoint_length = 0;

    while (cursor != LT_NIL){
        LT_Value element;

        if (!LT_Pair_p(cursor)){
            LT_error("Expected proper list of characters");
        }

        element = LT_car(cursor);
        StringBuilder_append_codepoint(builder, LT_Character_value(element));
        codepoint_length++;
        cursor = LT_cdr(cursor);
    }

    return String_new_normalized(
        LT_StringBuilder_value(builder),
        LT_StringBuilder_length(builder),
        codepoint_length
    );
}
