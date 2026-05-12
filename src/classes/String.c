/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/String.h>
#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/Dictionary.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Character.h>
#include <ListTalk/classes/IdentityDictionary.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/utils.h>
#include <ListTalk/utils/utf8.h>

#include <ctype.h>
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
    {"join:", &string_method_join},
    {"contains?:", &string_method_contains},
    {"startsWith?:", &string_method_starts_with},
    {"find:", &string_method_find},
    {"findAll:", &string_method_find_all},
    {"asByteVector", &string_method_as_bytevector},
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
