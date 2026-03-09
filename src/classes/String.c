/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/String.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>

#include <ctype.h>
#include <string.h>

struct LT_String_s {
    LT_Object base;
    size_t length;
    char str[];
};

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

LT_DEFINE_CLASS(LT_String) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "String",
    .instance_size = sizeof(LT_String),
    .debugPrintOn = String_debugPrintOn,
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
