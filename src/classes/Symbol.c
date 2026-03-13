/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Symbol.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/macros/arg_macros.h>

#include <ListTalk/utils.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct LT_Symbol_s {
    LT_Value package;
    char* name;
};

static LT_Slot_Descriptor Symbol_slots[] = {
    {"package", offsetof(LT_Symbol, package), &LT_SlotType_ReadonlyObject},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static char* symbol_name_designator(LT_Value value){
    if (LT_Symbol_p(value)){
        return LT_Symbol_name(LT_Symbol_from_value(value));
    }
    if (LT_String_p(value)){
        return (char*)LT_String_value_cstr(LT_String_from_value(value));
    }
    LT_error("Name designator must be symbol or string");
    return NULL;
}

static void Symbol_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Symbol* symbol = (LT_Symbol*)LT_VALUE_POINTER_VALUE(obj);
    LT_Package* package = LT_Symbol_package(symbol);
    LT_Package* current_package = LT_get_current_package();
    char* name = LT_Symbol_name(symbol);
    int omit_prefix = 0;

    if (package == LT_PACKAGE_KEYWORD){
        fputc(':', stream);
        fputs(name, stream);
        return;
    }
    if (package == NULL){
        if (name != NULL){
            fputs("#:", stream);
            fputs(name, stream);
            return;
        }
        fprintf(stream, "#<gensym at %p>", (void*)symbol);
        return;
    }

    if (package == current_package){
        omit_prefix = 1;
    } else if (current_package != NULL
        && package != NULL
        && LT_Package_uses_package(current_package, package)
        && LT_Package_lookup_local_symbol(current_package, name) == LT_INVALID){
        omit_prefix = 1;
    }

    if (!omit_prefix && package != NULL){
        fputs(LT_Package_name(package), stream);
        fputc(':', stream);
    }
    fputs(name, stream);
}

LT_DEFINE_PRIMITIVE(
    symbol_class_method_gensym,
    "Symbol class>>gensym",
    "(self [name])",
    "Return a fresh gensym or named uninterned symbol."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value name_designator = LT_NIL;
    char* name = NULL;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG_OPT(cursor, name_designator, LT_NIL);
    LT_ARG_END(cursor);

    if (self != (LT_Value)(uintptr_t)&LT_Symbol_class){
        LT_error("gensym class method is only supported on Symbol");
    }

    if (name_designator != LT_NIL){
        name = symbol_name_designator(name_designator);
    }
    return LT_Symbol_gensym(name);
}

LT_DEFINE_PRIMITIVE(
    symbol_class_method_uninterned,
    "Symbol class>>uninterned:",
    "(self name)",
    "Return a fresh uninterned symbol with the provided name."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value name_designator;
    char* name;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, name_designator);
    LT_ARG_END(cursor);

    if (self != (LT_Value)(uintptr_t)&LT_Symbol_class){
        LT_error("uninterned: class method is only supported on Symbol");
    }

    name = symbol_name_designator(name_designator);
    return LT_Symbol_new_uninterned(name);
}

static LT_Method_Descriptor Symbol_class_methods[] = {
    {"gensym", &symbol_class_method_gensym},
    {"uninterned:", &symbol_class_method_uninterned},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Symbol) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Symbol",
    .class_flags = LT_CLASS_FLAG_SPECIAL,
    .instance_size = sizeof(LT_Symbol),
    .debugPrintOn = Symbol_debugPrintOn,
    .slots = Symbol_slots,
    .class_methods = Symbol_class_methods,
};

LT_Value LT__Symbol_new_uninterned(LT_Package* package, char *name){
    LT_Symbol* symbol;

    symbol = GC_NEW(LT_Symbol);
    if (package == NULL){
        symbol->package = LT_NIL;
    } else {
        symbol->package = (LT_Value)(uintptr_t)package;
    }
    if (name == NULL){
        symbol->name = NULL;
    } else {
        symbol->name = LT_strdup(name);
    }
    return ((LT_Value)(uintptr_t)symbol) | LT_VALUE_POINTER_TAG_SYMBOL;
}

LT_Value LT_Symbol_new_uninterned(char* name){
    return LT__Symbol_new_uninterned(NULL, name);
}

LT_Value LT_Symbol_gensym(char* name){
    return LT_Symbol_new_uninterned(name);
}

LT_Value LT_Symbol_new_in(LT_Package* package, char *name){
    return LT_Package_intern_local_symbol(package, name);
}

LT_Value LT_Symbol_new(char *name){
    return LT_Symbol_new_in(LT_get_current_package(), name);
}

LT_Value LT_Symbol_parse_token(char* token){
    char* last_colon;

    if (token == NULL){
        LT_error("Symbol token must not be NULL");
    }

    if (token[0] == ':'){
        if (token[1] == '\0'){
            LT_error("Keyword symbol must not be empty");
        }
        return LT_Symbol_new_in(LT_PACKAGE_KEYWORD, token + 1);
    }

    last_colon = strrchr(token, ':');
    if (last_colon == NULL){
        return LT_Package_intern_symbol(LT_get_current_package(), token);
    }

    if (last_colon[1] == '\0'){
        LT_error("Package-prefixed symbol must have non-empty name");
    }

    {
        size_t package_len = (size_t)(last_colon - token);
        char* package_name = GC_MALLOC_ATOMIC(package_len + 1);
        LT_Package* package;

        memcpy(package_name, token, package_len);
        package_name[package_len] = '\0';
        package = LT_Package_resolve_used_package(
            LT_get_current_package(),
            package_name
        );
        if (package == NULL){
            package = LT_Package_new(package_name);
        }
        return LT_Symbol_new_in(package, last_colon + 1);
    }
}

char *LT_Symbol_name(LT_Symbol * symbol)
{
    return symbol->name;
}

LT_Package* LT_Symbol_package(LT_Symbol* symbol){
    if (symbol->package == LT_NIL){
        return NULL;
    }
    return (LT_Package*)LT_VALUE_POINTER_VALUE(symbol->package);
}
