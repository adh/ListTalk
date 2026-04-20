/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/String.h>
#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Character.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/utils.h>

#include <ctype.h>
#include <stdint.h>
#include <string.h>

struct LT_String_s {
    LT_Object base;
    size_t length;
    size_t byte_length;
    char str[];
};

static int utf8_is_continuation(unsigned char ch){
    return (ch & 0xc0) == 0x80;
}

static int utf8_continuation_valid(unsigned char ch){
    return ch != '\0' && utf8_is_continuation(ch);
}

static size_t utf8_sequence_length(unsigned char first){
    if (first < 0x80){
        return 1;
    }
    if (first >= 0xc2 && first <= 0xdf){
        return 2;
    }
    if (first >= 0xe0 && first <= 0xef){
        return 3;
    }
    if (first >= 0xf0 && first <= 0xf4){
        return 4;
    }
    return 0;
}

static int utf8_sequence_valid(const unsigned char* cursor, size_t length){
    unsigned char first = cursor[0];

    switch (length){
        case 1:
            return first < 0x80;
        case 2:
            return utf8_continuation_valid(cursor[1]);
        case 3:
            if (!utf8_continuation_valid(cursor[1])
                || !utf8_continuation_valid(cursor[2])){
                return 0;
            }
            if (first == 0xe0){
                return cursor[1] >= 0xa0;
            }
            if (first == 0xed){
                return cursor[1] <= 0x9f;
            }
            return 1;
        case 4:
            if (!utf8_continuation_valid(cursor[1])
                || !utf8_continuation_valid(cursor[2])
                || !utf8_continuation_valid(cursor[3])){
                return 0;
            }
            if (first == 0xf0){
                return cursor[1] >= 0x90;
            }
            if (first == 0xf4){
                return cursor[1] <= 0x8f;
            }
            return 1;
        default:
            return 0;
    }
}

static int utf8_sequence_valid_bounded(const unsigned char* cursor,
                                       size_t available,
                                       size_t length){
    if (length > available){
        return 0;
    }
    return utf8_sequence_valid(cursor, length);
}

static void StringBuilder_append_codepoint(LT_StringBuilder* builder,
                                           uint32_t codepoint){
    if (codepoint <= UINT32_C(0x7f)){
        LT_StringBuilder_append_char(builder, (char)codepoint);
    } else if (codepoint <= UINT32_C(0x7ff)){
        LT_StringBuilder_append_char(
            builder,
            (char)(0xc0 | (codepoint >> 6))
        );
        LT_StringBuilder_append_char(
            builder,
            (char)(0x80 | (codepoint & 0x3f))
        );
    } else if (codepoint <= UINT32_C(0xffff)){
        LT_StringBuilder_append_char(
            builder,
            (char)(0xe0 | (codepoint >> 12))
        );
        LT_StringBuilder_append_char(
            builder,
            (char)(0x80 | ((codepoint >> 6) & 0x3f))
        );
        LT_StringBuilder_append_char(
            builder,
            (char)(0x80 | (codepoint & 0x3f))
        );
    } else {
        LT_StringBuilder_append_char(
            builder,
            (char)(0xf0 | (codepoint >> 18))
        );
        LT_StringBuilder_append_char(
            builder,
            (char)(0x80 | ((codepoint >> 12) & 0x3f))
        );
        LT_StringBuilder_append_char(
            builder,
            (char)(0x80 | ((codepoint >> 6) & 0x3f))
        );
        LT_StringBuilder_append_char(
            builder,
            (char)(0x80 | (codepoint & 0x3f))
        );
    }
}

static uint32_t utf8_decode_valid(const unsigned char* cursor, size_t length){
    switch (length){
        case 1:
            return cursor[0];
        case 2:
            return ((uint32_t)(cursor[0] & 0x1f) << 6)
                | (uint32_t)(cursor[1] & 0x3f);
        case 3:
            return ((uint32_t)(cursor[0] & 0x0f) << 12)
                | ((uint32_t)(cursor[1] & 0x3f) << 6)
                | (uint32_t)(cursor[2] & 0x3f);
        case 4:
            return ((uint32_t)(cursor[0] & 0x07) << 18)
                | ((uint32_t)(cursor[1] & 0x3f) << 12)
                | ((uint32_t)(cursor[2] & 0x3f) << 6)
                | (uint32_t)(cursor[3] & 0x3f);
        default:
            return UINT32_C(0xfffd);
    }
}

static uint32_t utf8_codepoint_at_bounded(const char* cursor, size_t available){
    const unsigned char* bytes = (const unsigned char*)cursor;
    size_t length;

    if (available == 0){
        return UINT32_C(0xfffd);
    }
    if (bytes[0] == '\0'){
        return 0;
    }

    length = utf8_sequence_length(bytes[0]);
    if (length == 0 || !utf8_sequence_valid_bounded(bytes, available, length)){
        return UINT32_C(0xfffd);
    }

    return utf8_decode_valid(bytes, length);
}

static const char* utf8_next_bounded(const char* cursor, size_t available){
    const unsigned char* bytes = (const unsigned char*)cursor;
    size_t length;

    if (available == 0){
        return cursor;
    }
    if (bytes[0] == '\0'){
        return cursor + 1;
    }

    if (utf8_is_continuation(bytes[0])){
        size_t offset = 1;

        while (offset < available && utf8_is_continuation(bytes[offset])){
            offset++;
        }
        return cursor + offset;
    }

    length = utf8_sequence_length(bytes[0]);
    if (length == 0 || !utf8_sequence_valid_bounded(bytes, available, length)){
        return cursor + 1;
    }

    return cursor + length;
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
    return LT_SmallInteger_new((int64_t)LT_String_length(string));
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
        (size_t)LT_SmallInteger_value(index)
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
    int64_t from_value;
    int64_t to_value;
    LT_String* string;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, from);
    LT_OBJECT_ARG(cursor, to);
    LT_ARG_END(cursor);

    from_value = LT_SmallInteger_value(from);
    to_value = LT_SmallInteger_value(to);
    if (from_value < 0 || to_value < 0){
        LT_error("String index out of bounds");
    }

    string = LT_String_from_value(self);
    return (LT_Value)(uintptr_t)LT_String_substring(
        string,
        (size_t)from_value,
        (size_t)to_value
    );
}

static LT_Method_Descriptor String_methods[] = {
    {"length", &string_method_length},
    {"at:", &string_method_at},
    {"append:", &string_method_append},
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

        codepoint = utf8_codepoint_at_bounded(cursor, available);
        next = utf8_next_bounded(cursor, available);

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
    const unsigned char* bytes = (const unsigned char*)cursor;
    size_t length;

    if (bytes[0] == '\0'){
        return 0;
    }

    length = utf8_sequence_length(bytes[0]);
    if (length == 0 || !utf8_sequence_valid(bytes, length)){
        return UINT32_C(0xfffd);
    }

    return utf8_decode_valid(bytes, length);
}

const char* LT_String_utf8_next(const char* cursor){
    const unsigned char* bytes = (const unsigned char*)cursor;
    size_t length;

    if (bytes[0] == '\0'){
        return cursor + 1;
    }

    if (utf8_is_continuation(bytes[0])){
        do {
            cursor++;
            bytes++;
        } while (bytes[0] != '\0' && utf8_is_continuation(bytes[0]));
        return cursor;
    }

    length = utf8_sequence_length(bytes[0]);
    if (length == 0 || !utf8_sequence_valid(bytes, length)){
        return cursor + 1;
    }

    return cursor + length;
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
