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

struct LT_RelativePathname_s {
    LT_Pathname base;
};

struct LT_AbsolutePathname_s {
    LT_Pathname base;
};

static void Pathname_check_string(LT_String* string){
    if (strlen(LT_String_value_cstr(string)) != LT_String_byte_length(string)){
        LT_error("Pathname cannot contain NUL bytes");
    }
}

static char* Pathname_normalize(const char* input, int absolute){
    size_t input_length = strlen(input);
    char* copy = GC_MALLOC_ATOMIC(input_length + 1);
    char** segments = GC_MALLOC(sizeof(char*) * (input_length + 1));
    size_t count = 0;
    char* cursor;
    char* output = GC_MALLOC_ATOMIC(input_length + 3);
    char* output_cursor = output;
    size_t index;

    memcpy(copy, input, input_length + 1);
    cursor = copy;
    while (*cursor != '\0'){
        char* segment;

        while (*cursor == '/'){
            cursor++;
        }
        if (*cursor == '\0'){
            break;
        }
        segment = cursor;
        while (*cursor != '\0' && *cursor != '/'){
            cursor++;
        }
        if (*cursor != '\0'){
            *cursor++ = '\0';
        }
        if (strcmp(segment, ".") == 0){
            continue;
        }
        if (strcmp(segment, "..") == 0){
            if (count > 0 && strcmp(segments[count - 1], "..") != 0){
                count--;
            } else if (absolute){
                LT_error("Absolute pathname cannot resolve above root");
            } else {
                segments[count++] = segment;
            }
        } else {
            segments[count++] = segment;
        }
    }

    if (absolute){
        *output_cursor++ = '/';
    } else if (count == 0){
        *output_cursor++ = '.';
    } else if (strcmp(segments[0], "..") != 0){
        *output_cursor++ = '.';
        *output_cursor++ = '/';
    }
    for (index = 0; index < count; index++){
        size_t length = strlen(segments[index]);

        if (index > 0){
            *output_cursor++ = '/';
        }
        memcpy(output_cursor, segments[index], length);
        output_cursor += length;
    }
    *output_cursor = '\0';
    return output;
}

static LT_Pathname* Pathname_allocate(LT_Class* klass, char* normalized){
    LT_Pathname* pathname = LT_Class_alloc(klass);

    pathname->pathname = normalized;
    return pathname;
}

static size_t Pathname_hash(LT_Value value){
    const unsigned char* cursor = (const unsigned char*)LT_Pathname_value_cstr(
        LT_Pathname_from_value(value)
    );
    uint32_t hash = UINT32_C(0x811c9dc5);

    while (*cursor != '\0'){
        hash ^= *cursor++;
        hash *= UINT32_C(0x01000193);
    }
    return (size_t)hash;
}

static int Pathname_equal_p(LT_Value left, LT_Value right){
    return LT_Pathname_p(right)
        && strcmp(
            LT_Pathname_value_cstr(LT_Pathname_from_value(left)),
            LT_Pathname_value_cstr(LT_Pathname_from_value(right))
        ) == 0;
}

static void Pathname_debugPrintOn(LT_Value value, FILE* stream){
    LT_String* string = LT_Pathname_as_string(LT_Pathname_from_value(value));

    fputs("#p", stream);
    LT_Value_debugPrintOn((LT_Value)(uintptr_t)string, stream);
}

typedef LT_Pathname* (*Pathname_StringConstructor)(LT_String* string);

static LT_Pathname* RelativePathname_from_string_as_pathname(LT_String* string){
    return (LT_Pathname*)LT_RelativePathname_from_string(string);
}

static LT_Pathname* AbsolutePathname_from_string_as_pathname(LT_String* string){
    return (LT_Pathname*)LT_AbsolutePathname_from_string(string);
}

static LT_Value Pathname_class_from_string(LT_Value arguments,
                                           LT_Class* expected_class,
                                           Pathname_StringConstructor constructor){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* string;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, string, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)expected_class){
        LT_error("fromString: is not supported on this class");
    }
    return (LT_Value)(uintptr_t)constructor(string);
}

LT_DEFINE_PRIMITIVE(
    pathname_class_method_from_string,
    "Pathname class>>fromString:",
    "(self string)",
    "Create an absolute or relative pathname according to the string's shape."
){
    (void)tail_call_unwind_marker;
    return Pathname_class_from_string(
        arguments, &LT_Pathname_class, LT_Pathname_from_string
    );
}

LT_DEFINE_PRIMITIVE(
    relative_pathname_class_method_from_string,
    "RelativePathname class>>fromString:",
    "(self string)",
    "Create a relative pathname; reject a leading slash."
){
    (void)tail_call_unwind_marker;
    return Pathname_class_from_string(
        arguments,
        &LT_RelativePathname_class,
        RelativePathname_from_string_as_pathname
    );
}

LT_DEFINE_PRIMITIVE(
    absolute_pathname_class_method_from_string,
    "AbsolutePathname class>>fromString:",
    "(self string)",
    "Create an absolute pathname, adding a leading slash when absent."
){
    (void)tail_call_unwind_marker;
    return Pathname_class_from_string(
        arguments,
        &LT_AbsolutePathname_class,
        AbsolutePathname_from_string_as_pathname
    );
}

LT_DEFINE_PRIMITIVE(
    pathname_method_absolute_p,
    "Pathname>>absolute?",
    "(self)",
    "Return true when the pathname is absolute."
){
    LT_Value cursor = arguments;
    LT_Pathname* pathname;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, pathname, LT_Pathname*, LT_Pathname_from_value);
    LT_ARG_END(cursor);
    return LT_Pathname_absolute_p(pathname) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    pathname_method_relative_p,
    "Pathname>>relative?",
    "(self)",
    "Return true when the pathname is relative."
){
    LT_Value cursor = arguments;
    LT_Pathname* pathname;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, pathname, LT_Pathname*, LT_Pathname_from_value);
    LT_ARG_END(cursor);
    return LT_Pathname_relative_p(pathname) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    pathname_method_append,
    "Pathname>>/",
    "(self pathname)",
    "Append a relative pathname to the receiver and normalize the result."
){
    LT_Value cursor = arguments;
    LT_Pathname* left;
    LT_Pathname* right;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, left, LT_Pathname*, LT_Pathname_from_value);
    LT_GENERIC_ARG(cursor, right, LT_Pathname*, LT_Pathname_from_value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_Pathname_append(left, right);
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

LT_DEFINE_PRIMITIVE(
    pathname_method_parent,
    "Pathname>>parent",
    "(self)",
    "Return the pathname with its final segment removed."
){
    LT_Value cursor = arguments;
    LT_Pathname* pathname;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, pathname, LT_Pathname*, LT_Pathname_from_value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_Pathname_parent(pathname);
}

LT_DEFINE_PRIMITIVE(
    absolute_pathname_method_rooted_at,
    "AbsolutePathname>>rootedAt:",
    "(self root)",
    "Interpret the absolute pathname relative to root."
){
    LT_Value cursor = arguments;
    LT_AbsolutePathname* pathname;
    LT_Pathname* root;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, pathname, LT_AbsolutePathname*,
                   LT_AbsolutePathname_from_value);
    LT_GENERIC_ARG(cursor, root, LT_Pathname*, LT_Pathname_from_value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_AbsolutePathname_rooted_at(pathname, root);
}

static LT_Method_Descriptor Pathname_methods[] = {
    {"asString", &pathname_method_as_string},
    {"absolute?", &pathname_method_absolute_p},
    {"relative?", &pathname_method_relative_p},
    {"/", &pathname_method_append},
    {"parent", &pathname_method_parent},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor Pathname_class_methods[] = {
    {"fromString:", &pathname_class_method_from_string},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor RelativePathname_class_methods[] = {
    {"fromString:", &relative_pathname_class_method_from_string},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor AbsolutePathname_class_methods[] = {
    {"fromString:", &absolute_pathname_class_method_from_string},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor AbsolutePathname_methods[] = {
    {"rootedAt:", &absolute_pathname_method_rooted_at},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Pathname) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Pathname",
    .documentation = "Abstract normalized UTF-8 filesystem pathname.",
    .instance_size = sizeof(LT_Pathname),
    .class_flags = LT_CLASS_FLAG_ABSTRACT | LT_CLASS_FLAG_IMMUTABLE
        | LT_CLASS_FLAG_SCALAR,
    .hash = Pathname_hash,
    .equal_p = Pathname_equal_p,
    .debugPrintOn = Pathname_debugPrintOn,
    .methods = Pathname_methods,
    .class_methods = Pathname_class_methods,
};

LT_DEFINE_CLASS(LT_RelativePathname) {
    .superclass = &LT_Pathname_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "RelativePathname",
    .documentation = "Normalized relative filesystem pathname.",
    .instance_size = sizeof(LT_RelativePathname),
    .class_flags = LT_CLASS_FLAG_FINAL | LT_CLASS_FLAG_IMMUTABLE
        | LT_CLASS_FLAG_SCALAR,
    .debugPrintOn = Pathname_debugPrintOn,
    .class_methods = RelativePathname_class_methods,
};

LT_DEFINE_CLASS(LT_AbsolutePathname) {
    .superclass = &LT_Pathname_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "AbsolutePathname",
    .documentation = "Normalized absolute filesystem pathname.",
    .instance_size = sizeof(LT_AbsolutePathname),
    .class_flags = LT_CLASS_FLAG_FINAL | LT_CLASS_FLAG_IMMUTABLE
        | LT_CLASS_FLAG_SCALAR,
    .debugPrintOn = Pathname_debugPrintOn,
    .methods = AbsolutePathname_methods,
    .class_methods = AbsolutePathname_class_methods,
};

LT_Pathname* LT_Pathname_new(char* pathname){
    return pathname[0] == '/'
        ? (LT_Pathname*)LT_AbsolutePathname_new(pathname)
        : (LT_Pathname*)LT_RelativePathname_new(pathname);
}

LT_Pathname* LT_Pathname_from_string(LT_String* string){
    Pathname_check_string(string);
    return LT_String_value_cstr(string)[0] == '/'
        ? (LT_Pathname*)LT_AbsolutePathname_from_string(string)
        : (LT_Pathname*)LT_RelativePathname_from_string(string);
}

LT_RelativePathname* LT_RelativePathname_new(char* pathname){
    return LT_RelativePathname_from_string(LT_String_new_cstr(pathname));
}

LT_RelativePathname* LT_RelativePathname_from_string(LT_String* string){
    Pathname_check_string(string);
    if (LT_String_value_cstr(string)[0] == '/'){
        LT_error("Relative pathname cannot start with slash");
    }
    return (LT_RelativePathname*)Pathname_allocate(
        &LT_RelativePathname_class,
        Pathname_normalize(LT_String_value_cstr(string), 0)
    );
}

LT_AbsolutePathname* LT_AbsolutePathname_new(char* pathname){
    return LT_AbsolutePathname_from_string(LT_String_new_cstr(pathname));
}

LT_AbsolutePathname* LT_AbsolutePathname_from_string(LT_String* string){
    Pathname_check_string(string);
    return (LT_AbsolutePathname*)Pathname_allocate(
        &LT_AbsolutePathname_class,
        Pathname_normalize(LT_String_value_cstr(string), 1)
    );
}

LT_Pathname* LT_Pathname_append(LT_Pathname* left, LT_Pathname* right){
    const char* left_bytes = LT_Pathname_value_cstr(left);
    const char* right_bytes = LT_Pathname_value_cstr(right);
    const char* right_suffix;
    size_t left_length;
    size_t right_length;
    char* combined;

    if (LT_Pathname_absolute_p(right)){
        LT_error("Cannot append an absolute pathname");
    }
    right_suffix = strcmp(right_bytes, ".") == 0
        ? ""
        : (right_bytes[0] == '.' && right_bytes[1] == '/'
            ? right_bytes + 2
            : right_bytes);
    if (strcmp(left_bytes, ".") == 0){
        return LT_Pathname_new((char*)(*right_suffix == '\0' ? "." : right_suffix));
    }
    left_length = strlen(left_bytes);
    right_length = strlen(right_suffix);
    combined = GC_MALLOC_ATOMIC(left_length + right_length + 2);
    memcpy(combined, left_bytes, left_length);
    combined[left_length] = '/';
    memcpy(combined + left_length + 1, right_suffix, right_length + 1);
    return LT_Pathname_new(combined);
}

LT_Pathname* LT_Pathname_parent(LT_Pathname* pathname){
    const char* bytes = LT_Pathname_value_cstr(pathname);
    const char* slash;
    size_t length;
    char* parent;

    if (strcmp(bytes, ".") == 0 || strcmp(bytes, "/") == 0){
        LT_error("Pathname has no parent");
    }
    slash = strrchr(bytes, '/');
    if (slash == NULL){
        return LT_Pathname_new(".");
    }
    if (slash == bytes){
        return LT_Pathname_new("/");
    }
    length = (size_t)(slash - bytes);
    if (length == 1 && bytes[0] == '.'){
        return LT_Pathname_new(".");
    }
    parent = GC_MALLOC_ATOMIC(length + 1);
    memcpy(parent, bytes, length);
    parent[length] = '\0';
    return LT_Pathname_new(parent);
}

LT_Pathname* LT_AbsolutePathname_rooted_at(LT_AbsolutePathname* pathname,
                                           LT_Pathname* root){
    const char* suffix = LT_Pathname_value_cstr((LT_Pathname*)pathname) + 1;
    LT_RelativePathname* relative = LT_RelativePathname_new(
        (char*)(*suffix == '\0' ? "." : suffix)
    );

    return LT_Pathname_append(root, (LT_Pathname*)relative);
}

LT_String* LT_Pathname_as_string(LT_Pathname* pathname){
    return LT_String_new_cstr(pathname->pathname);
}

char* LT_Pathname_value_cstr(LT_Pathname* pathname){
    return pathname->pathname;
}

int LT_Pathname_absolute_p(LT_Pathname* pathname){
    return LT_AbsolutePathname_p((LT_Value)(uintptr_t)pathname);
}

int LT_Pathname_relative_p(LT_Pathname* pathname){
    return LT_RelativePathname_p((LT_Value)(uintptr_t)pathname);
}

char* LT_Pathname_like_value_cstr(LT_Value value){
    if (LT_Pathname_p(value)){
        return LT_Pathname_value_cstr(LT_Pathname_from_value(value));
    }
    if (LT_String_p(value)){
        LT_String* string = LT_String_from_value(value);
        Pathname_check_string(string);
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
