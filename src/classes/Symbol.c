/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Symbol.h>
#include <ListTalk/vm/Class.h>

#include <ListTalk/utils.h>

#include <string.h>

struct LT_Symbol_s {
    LT_Package* package;
    char* name;
};

static void Symbol_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Symbol* symbol = (LT_Symbol*)LT_VALUE_POINTER_VALUE(obj);
    LT_Package* package = LT_Symbol_package(symbol);
    char* name = LT_Symbol_name(symbol);

    if (package == LT_PACKAGE_KEYWORD){
        fputc(':', stream);
        fputs(name, stream);
        return;
    }
    if (package == LT_PACKAGE_LISTTALK){
        fputs(name, stream);
        return;
    }
    if (package != NULL){
        fputs(LT_Package_name(package), stream);
        fputc(':', stream);
    }
    fputs(name, stream);
}

LT_DEFINE_CLASS(LT_Symbol) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Symbol",
    .class_flags = LT_CLASS_FLAG_SPECIAL,
    .instance_size = sizeof(LT_Symbol),
    .debugPrintOn = Symbol_debugPrintOn,
};

LT_Value LT__Symbol_new_uninterned(LT_Package* package, char *name){
    LT_Symbol* symbol;

    if (package == NULL){
        LT_error("Symbol package must not be NULL");
    }
    if (name == NULL){
        LT_error("Symbol name must not be NULL");
    }

    symbol = GC_NEW(LT_Symbol);
    symbol->package = package;
    symbol->name = LT_strdup(name);
    return ((LT_Value)(uintptr_t)symbol) | LT_VALUE_POINTER_TAG_SYMBOL;
}

LT_Value LT_Symbol_new_in(LT_Package* package, char *name){
    return LT_Package_intern_symbol(package, name);
}

LT_Value LT_Symbol_new(char *name){
    return LT_Symbol_new_in(LT_PACKAGE_LISTTALK, name);
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
        return LT_Symbol_new(token);
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
        package = LT_Package_new(package_name);
        return LT_Symbol_new_in(package, last_colon + 1);
    }
}

char *LT_Symbol_name(LT_Symbol * symbol)
{
    return symbol->name;
}

LT_Package* LT_Symbol_package(LT_Symbol* symbol){
    return symbol->package;
}
