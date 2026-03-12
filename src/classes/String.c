/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/String.h>
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
    char str[];
};

static size_t String_hash(LT_Value value){
    LT_String* string = LT_String_from_value(value);
    const unsigned char* cursor =
        (const unsigned char*)LT_String_value_cstr(string);
    size_t index;
    uint32_t hash = UINT32_C(0x811c9dc5);

    for (index = 0; index < LT_String_length(string); index++){
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
    length = LT_String_length(left_string);

    if (length != LT_String_length(right_string)){
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

    fputc('"', stream);
    while (*cursor != '\0'){
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
                if (isprint(ch)){
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
    "Return byte string length."
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
    "Return byte at index as character."
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

static LT_Method_Descriptor String_methods[] = {
    {"length", &string_method_length},
    {"at:", &string_method_at},
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
    LT_String* str = LT_Class_ALLOC_FLEXIBLE(LT_String, len + 1);
    str->length = len;
    memcpy(str->str, buf, len);
    str->str[len] = '\0';
    return str;
}
LT_String* LT_String_new_cstr(char* str){
    return LT_String_new(str, strlen(str));
}
const char* LT_String_value_cstr(LT_String* string){
    return string->str;
}

size_t LT_String_length(LT_String* string){
    return string->length;
}

unsigned char LT_String_at(LT_String* string, size_t index){
    if (index >= string->length){
        LT_error("String index out of bounds");
    }
    return (unsigned char)string->str[index];
}

LT_Value LT_String_to_character_list(LT_String* string){
    LT_Value list = LT_NIL;
    size_t index = string->length;

    while (index > 0){
        index--;
        list = LT_cons(
            LT_Character_new((uint32_t)(unsigned char)string->str[index]),
            list
        );
    }
    return list;
}

LT_String* LT_String_from_character_list(LT_Value characters){
    LT_StringBuilder* builder = LT_StringBuilder_new();
    LT_Value cursor = characters;

    while (cursor != LT_NIL){
        LT_Value element;

        if (!LT_Pair_p(cursor)){
            LT_error("Expected proper list of characters");
        }

        element = LT_car(cursor);
        if (LT_Character_value(element) > UINT32_C(0xff)){
            LT_error(
                "Cannot encode character code point above 0xFF in byte string"
            );
        }
        LT_StringBuilder_append_char(
            builder,
            (char)(unsigned char)LT_Character_value(element)
        );
        cursor = LT_cdr(cursor);
    }

    return LT_String_new(
        LT_StringBuilder_value(builder),
        LT_StringBuilder_length(builder)
    );
}
