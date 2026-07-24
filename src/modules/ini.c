/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Boolean.h>
#include <ListTalk/classes/Package.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/utils/ini.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/loader.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

LT_DECLARE_CLASS(LT_INIReader);

struct LT_INIReader_s {
    LT_Object base;
    LT_Value duplicate_policy;
    LT_Value allow_global_keys;
    LT_Value allow_empty_values;
};

static LT_Value ini_keyword(const char* name){
    return LT_Symbol_new_in(LT_PACKAGE_KEYWORD, (char*)name);
}

static int ini_keyword_p(LT_Value value, const char* name){
    LT_Symbol* symbol;

    if (!LT_Symbol_p(value)){
        return 0;
    }
    symbol = LT_Symbol_from_value(value);
    return LT_Symbol_package(symbol) == LT_PACKAGE_KEYWORD
        && strcmp(LT_Symbol_name(symbol), name) == 0;
}

static LT_INIReader* ini_reader_new(void){
    LT_INIReader* reader = LT_Class_ALLOC(LT_INIReader);

    reader->duplicate_policy = ini_keyword("last-wins");
    reader->allow_global_keys = LT_TRUE;
    reader->allow_empty_values = LT_TRUE;
    return reader;
}

static int ini_reader_duplicate_policy(LT_INIReader* reader){
    if (ini_keyword_p(reader->duplicate_policy, "last-wins")){
        return LT_INI_DUPLICATE_LAST_WINS;
    }
    if (ini_keyword_p(reader->duplicate_policy, "first-wins")){
        return LT_INI_DUPLICATE_FIRST_WINS;
    }
    if (ini_keyword_p(reader->duplicate_policy, "error")){
        return LT_INI_DUPLICATE_ERROR;
    }
    LT_error("Invalid INI duplicate policy");
    return LT_INI_DUPLICATE_LAST_WINS;
}

static unsigned int ini_reader_flags(LT_INIReader* reader){
    unsigned int flags = 0;

    if (LT_Value_truthy_p(reader->allow_global_keys)){
        flags |= LT_INI_ALLOW_GLOBAL_KEYS;
    }
    if (LT_Value_truthy_p(reader->allow_empty_values)){
        flags |= LT_INI_ALLOW_EMPTY_VALUES;
    }
    return flags;
}

static void INIReader_debugPrintOn(LT_Value obj, FILE* stream){
    LT_INIReader* reader = LT_INIReader_from_value(obj);

    fprintf(stream, "#<INIReader %p>", (void*)reader);
}

LT_DEFINE_PRIMITIVE(
    ini_reader_class_method_new,
    "INIReader class>>new",
    "(self)",
    "Create an INI reader."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    if (self != LT_STATIC_CLASS(LT_INIReader)){
        LT_error("new class method is only supported on INIReader");
    }
    return (LT_Value)(uintptr_t)ini_reader_new();
}

LT_DEFINE_PRIMITIVE(
    ini_reader_class_method_read,
    "INIReader class>>read:",
    "(self string)",
    "Parse an INI string using the default reader."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* string;
    LT_INIReader* reader;
    LT_INI* ini;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, string, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);
    if (self != LT_STATIC_CLASS(LT_INIReader)){
        LT_error("read: class method is only supported on INIReader");
    }

    reader = ini_reader_new();
    ini = LT_INI_parseString(
        "<string>",
        string,
        ini_reader_flags(reader),
        ini_reader_duplicate_policy(reader)
    );
    return LT_INI_asDictionary(ini);
}

LT_DEFINE_PRIMITIVE(
    ini_reader_class_method_read_file,
    "INIReader class>>readFile:",
    "(self path)",
    "Parse an INI file using the default reader."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* path;
    LT_INIReader* reader;
    LT_INI* ini;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, path, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);
    if (self != LT_STATIC_CLASS(LT_INIReader)){
        LT_error("readFile: class method is only supported on INIReader");
    }

    reader = ini_reader_new();
    ini = LT_INI_parseFile(
        LT_String_value_cstr(path),
        ini_reader_flags(reader),
        ini_reader_duplicate_policy(reader)
    );
    return LT_INI_asDictionary(ini);
}

LT_DEFINE_PRIMITIVE(
    ini_reader_method_read,
    "INIReader>>read:",
    "(self string)",
    "Parse an INI string."
){
    LT_Value cursor = arguments;
    LT_INIReader* self;
    LT_String* string;
    LT_INI* ini;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_INIReader*, LT_INIReader_from_value);
    LT_GENERIC_ARG(cursor, string, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);

    ini = LT_INI_parseString(
        "<string>",
        string,
        ini_reader_flags(self),
        ini_reader_duplicate_policy(self)
    );
    return LT_INI_asDictionary(ini);
}

LT_DEFINE_PRIMITIVE(
    ini_reader_method_read_file,
    "INIReader>>readFile:",
    "(self path)",
    "Parse an INI file."
){
    LT_Value cursor = arguments;
    LT_INIReader* self;
    LT_String* path;
    LT_INI* ini;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_INIReader*, LT_INIReader_from_value);
    LT_GENERIC_ARG(cursor, path, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);

    ini = LT_INI_parseFile(
        LT_String_value_cstr(path),
        ini_reader_flags(self),
        ini_reader_duplicate_policy(self)
    );
    return LT_INI_asDictionary(ini);
}

LT_DEFINE_PRIMITIVE(
    ini_reader_method_duplicate_policy,
    "INIReader>>duplicatePolicy",
    "(self)",
    "Return duplicate key policy."
){
    LT_Value cursor = arguments;
    LT_INIReader* self;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_INIReader*, LT_INIReader_from_value);
    LT_ARG_END(cursor);
    return self->duplicate_policy;
}

LT_DEFINE_PRIMITIVE(
    ini_reader_method_duplicate_policy_set,
    "INIReader>>duplicatePolicy:",
    "(self policy)",
    "Set duplicate key policy to :last-wins, :first-wins, or :error."
){
    LT_Value cursor = arguments;
    LT_INIReader* self;
    LT_Value policy;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_INIReader*, LT_INIReader_from_value);
    LT_OBJECT_ARG(cursor, policy);
    LT_ARG_END(cursor);
    if (!ini_keyword_p(policy, "last-wins")
        && !ini_keyword_p(policy, "first-wins")
        && !ini_keyword_p(policy, "error")){
        LT_error("Invalid INI duplicate policy");
    }
    self->duplicate_policy = policy;
    return (LT_Value)(uintptr_t)self;
}

LT_DEFINE_PRIMITIVE(
    ini_reader_method_allow_global_keys,
    "INIReader>>allowGlobalKeys?",
    "(self)",
    "Return whether keys before a section are allowed."
){
    LT_Value cursor = arguments;
    LT_INIReader* self;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_INIReader*, LT_INIReader_from_value);
    LT_ARG_END(cursor);
    return self->allow_global_keys;
}

LT_DEFINE_PRIMITIVE(
    ini_reader_method_allow_global_keys_set,
    "INIReader>>allowGlobalKeys?:",
    "(self allow)",
    "Set whether keys before a section are allowed."
){
    LT_Value cursor = arguments;
    LT_INIReader* self;
    LT_Value allow;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_INIReader*, LT_INIReader_from_value);
    LT_OBJECT_ARG(cursor, allow);
    LT_ARG_END(cursor);
    self->allow_global_keys = LT_Value_truthy_p(allow) ? LT_TRUE : LT_FALSE;
    return (LT_Value)(uintptr_t)self;
}

LT_DEFINE_PRIMITIVE(
    ini_reader_method_allow_empty_values,
    "INIReader>>allowEmptyValues?",
    "(self)",
    "Return whether empty values are allowed."
){
    LT_Value cursor = arguments;
    LT_INIReader* self;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_INIReader*, LT_INIReader_from_value);
    LT_ARG_END(cursor);
    return self->allow_empty_values;
}

LT_DEFINE_PRIMITIVE(
    ini_reader_method_allow_empty_values_set,
    "INIReader>>allowEmptyValues?:",
    "(self allow)",
    "Set whether empty values are allowed."
){
    LT_Value cursor = arguments;
    LT_INIReader* self;
    LT_Value allow;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_INIReader*, LT_INIReader_from_value);
    LT_OBJECT_ARG(cursor, allow);
    LT_ARG_END(cursor);
    self->allow_empty_values = LT_Value_truthy_p(allow) ? LT_TRUE : LT_FALSE;
    return (LT_Value)(uintptr_t)self;
}

static LT_Slot_Descriptor INIReader_slots[] = {
    {"duplicatePolicy", offsetof(LT_INIReader, duplicate_policy), &LT_SlotType_Object},
    {"allowGlobalKeys", offsetof(LT_INIReader, allow_global_keys), &LT_SlotType_Object},
    {"allowEmptyValues", offsetof(LT_INIReader, allow_empty_values), &LT_SlotType_Object},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static LT_Method_Descriptor INIReader_methods[] = {
    {"read:", &ini_reader_method_read},
    {"readFile:", &ini_reader_method_read_file},
    {"duplicatePolicy", &ini_reader_method_duplicate_policy},
    {"duplicatePolicy:", &ini_reader_method_duplicate_policy_set},
    {"allowGlobalKeys?", &ini_reader_method_allow_global_keys},
    {"allowGlobalKeys?:", &ini_reader_method_allow_global_keys_set},
    {"allowEmptyValues?", &ini_reader_method_allow_empty_values},
    {"allowEmptyValues?:", &ini_reader_method_allow_empty_values_set},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor INIReader_class_methods[] = {
    {"new", &ini_reader_class_method_new},
    {"read:", &ini_reader_class_method_read},
    {"readFile:", &ini_reader_class_method_read_file},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_INIReader) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "INIReader",
    .documentation = "Configuration object for reading INI files.",
    .instance_size = sizeof(LT_INIReader),
    .class_flags = LT_CLASS_FLAG_FINAL,
    .debugPrintOn = INIReader_debugPrintOn,
    .slots = INIReader_slots,
    .methods = INIReader_methods,
    .class_methods = INIReader_class_methods,
};

void ListTalk_ini_load(LT_Environment* environment){
    LT_Package* package = LT_Package_new("ListTalk-INI");

    LT_init_native_class(&LT_INIReader_class);
    LT_Environment_bind(
        environment,
        LT_Symbol_new_in(package, "INIReader"),
        LT_STATIC_CLASS(LT_INIReader),
        LT_ENV_BINDING_FLAG_CONSTANT
    );

    LT_loader_provide(environment, "ini");
}
