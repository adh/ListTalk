/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */
#include <ListTalk/classes/Class.h>
#include <ListTalk/classes/Object.h>
#include <ListTalk/classes/Pathname.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/error.h>

#include <gc.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct LT_Pathname_s {
    LT_Object base;
    char* pathname;
};

static void Pathname_debugPrintOn(LT_Value value, FILE* stream){
    fprintf(stream, "#<Pathname %s>",
            LT_Pathname_value_cstr(LT_Pathname_from_value(value)));
}

LT_DEFINE_PRIMITIVE(
    pathname_class_method_from_string,
    "Pathname class>>fromString:",
    "(self string)",
    "Create a pathname from a string that contains no NUL bytes."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* string;
    (void)tail_call_unwind_marker;
    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, string, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);
    if (self != LT_STATIC_CLASS(LT_Pathname)){
        LT_error("fromString: is only supported on Pathname");
    }
    return (LT_Value)(uintptr_t)LT_Pathname_from_string(string);
}

LT_DEFINE_PRIMITIVE(
    pathname_method_as_string,
    "Pathname>>asString",
    "(self)",
    "Return the pathname as a string."
){
    LT_Value cursor = arguments;
    LT_Pathname* pathname;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, pathname, LT_Pathname*, LT_Pathname_from_value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_Pathname_as_string(pathname);
}

static LT_Method_Descriptor Pathname_methods[] = {
    {"asString", &pathname_method_as_string},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor Pathname_class_methods[] = {
    {"fromString:", &pathname_class_method_from_string},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Pathname) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Pathname",
    .documentation = "Immutable NUL-terminated UTF-8 filesystem pathname.",
    .instance_size = sizeof(LT_Pathname),
    .class_flags = LT_CLASS_FLAG_IMMUTABLE | LT_CLASS_FLAG_SCALAR,
    .debugPrintOn = Pathname_debugPrintOn,
    .methods = Pathname_methods,
    .class_methods = Pathname_class_methods,
};

LT_Pathname* LT_Pathname_new(char* pathname){
    return LT_Pathname_from_string(LT_String_new_cstr(pathname));
}

LT_Pathname* LT_Pathname_from_string(LT_String* string){
    LT_Pathname* pathname;
    size_t length = LT_String_byte_length(string);
    char* bytes;

    if (strlen(LT_String_value_cstr(string)) != length){
        LT_error("Pathname cannot contain NUL bytes");
    }
    bytes = GC_MALLOC_ATOMIC(length + 1);
    memcpy(bytes, LT_String_value_cstr(string), length + 1);
    pathname = LT_Class_ALLOC(LT_Pathname);
    pathname->pathname = bytes;
    return pathname;
}

LT_String* LT_Pathname_as_string(LT_Pathname* pathname){
    return LT_String_new_cstr(pathname->pathname);
}

char* LT_Pathname_value_cstr(LT_Pathname* pathname){
    return pathname->pathname;
}

char* LT_Pathname_like_value_cstr(LT_Value value){
    if (LT_Pathname_p(value)){
        return LT_Pathname_value_cstr(LT_Pathname_from_value(value));
    }
    if (LT_String_p(value)){
        LT_String* string = LT_String_from_value(value);
        if (strlen(LT_String_value_cstr(string)) != LT_String_byte_length(string)){
            LT_error("Pathname cannot contain NUL bytes");
        }
        return (char*)LT_String_value_cstr(string);
    }
    LT_error("Expected Pathname or String");
    return NULL;
}

LT_String* LT_Pathname_like_as_string(LT_Value value){
    if (LT_Pathname_p(value)){
        return LT_Pathname_as_string(LT_Pathname_from_value(value));
    }
    (void)LT_Pathname_like_value_cstr(value);
    return LT_String_from_value(value);
}
