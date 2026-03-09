/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Package.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/vm/Class.h>

#include <ListTalk/utils.h>

extern LT_Value LT__Symbol_new_uninterned(LT_Package* package, char* name);

struct LT_Package_s {
    LT_Object base;
    char* name;
    LT_InlineHash symbol_table;
};

LT_Package LT_Package_LISTTALK = {0};
LT_Package LT_Package_KEYWORD = {0};
static int predefined_packages_initialized = 0;

static void Package_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Package* package = LT_Package_from_object(obj);
    if (package->name == NULL){
        fputs("#<Package>", stream);
        return;
    }
    fputs("#<Package ", stream);
    fputs(package->name, stream);
    fputc('>', stream);
}

LT_DEFINE_CLASS(LT_Package) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Package",
    .instance_size = sizeof(LT_Package),
    .debugPrintOn = Package_debugPrintOn,
};

static LT_InlineHash* get_package_table(void){
    static LT_InlineHash* package_table = NULL;

    if (package_table == NULL){
        package_table = GC_NEW(LT_InlineHash);
        LT_InlineHash_init(package_table);
    }

    return package_table;
}

static void package_init(LT_Package* package, char* name){
    package->base.klass = &LT_Package_class;
    package->name = LT_strdup(name);
    LT_InlineHash_init(&package->symbol_table);
}

static void ensure_predefined_packages_initialized(void){
    LT_InlineHash* package_table = get_package_table();

    if (predefined_packages_initialized){
        return;
    }
    predefined_packages_initialized = 1;

    package_init(&LT_Package_LISTTALK, "ListTalk");
    LT_StringHash_at_put(
        package_table,
        LT_Package_LISTTALK.name,
        &LT_Package_LISTTALK
    );

    package_init(&LT_Package_KEYWORD, "keyword");
    LT_StringHash_at_put(
        package_table,
        LT_Package_KEYWORD.name,
        &LT_Package_KEYWORD
    );
}

static void LT___init_predefined_packages(void){
    ensure_predefined_packages_initialized();
}
LT_REGISTER_INITIALIZER(LT___init_predefined_packages)

LT_Package* LT_Package_new(char* name){
    LT_InlineHash* package_table;
    LT_Package* package;

    if (name == NULL){
        LT_error("Package name must not be NULL");
    }

    ensure_predefined_packages_initialized();
    package_table = get_package_table();
    package = LT_StringHash_at(package_table, name);
    if (package != NULL){
        return package;
    }

    package = LT_Class_ALLOC(LT_Package);
    package_init(package, name);
    LT_StringHash_at_put(package_table, package->name, package);

    return package;
}

char* LT_Package_name(LT_Package* package){
    ensure_predefined_packages_initialized();
    return package->name;
}

LT_Value LT_Package_intern_symbol(LT_Package* package, char* name){
    LT_Symbol* symbol;
    LT_Value value;

    ensure_predefined_packages_initialized();
    if (package == NULL){
        LT_error("Symbol package must not be NULL");
    }
    if (name == NULL){
        LT_error("Symbol name must not be NULL");
    }

    symbol = LT_StringHash_at(&package->symbol_table, name);
    if (symbol != NULL){
        return ((LT_Value)(uintptr_t)symbol) | LT_VALUE_POINTER_TAG_SYMBOL;
    }

    value = LT__Symbol_new_uninterned(package, name);
    LT_StringHash_at_put(
        &package->symbol_table,
        LT_Symbol_name(LT_Symbol_from_object(value)),
        LT_VALUE_POINTER_VALUE(value)
    );
    return value;
}
