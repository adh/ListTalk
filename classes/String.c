/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/String.h>
#include <ListTalk/vm/Class.h>
#include <string.h>

struct LT_String_s {
    LT_Object base;
    size_t length;
    char str[];
};

LT_DEFINE_CLASS(LT_String) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "String",
    .instance_size = sizeof(LT_String),
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
char* LT_String_value_cstr(LT_String* string){
    return string->str;
}
