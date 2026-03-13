/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Package.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/vm/Class.h>

#include <ListTalk/utils.h>
#include <string.h>

extern LT_Value LT__Symbol_new_uninterned(LT_Package* package, char* name);

struct LT_Package_s {
    LT_Object base;
    char* name;
    LT_InlineHash symbol_table;
    LT_Value used_packages;
    LT_InlineHash used_package_nicknames;
};

LT_Package LT_Package_LISTTALK = {0};
LT_Package LT_Package_LISTTALK_IMPLEMENTATION = {0};
LT_Package LT_Package_LISTTALK_USER = {0};
LT_Package LT_Package_KEYWORD = {0};
static _Thread_local LT_Package* current_package = NULL;
static int predefined_packages_initialized = 0;

static void Package_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Package* package = LT_Package_from_value(obj);
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

int LT_Package_uses_package(LT_Package* package, LT_Package* used_package){
    LT_Value cursor = package->used_packages;

    while (LT_Pair_p(cursor)){
        if ((LT_Package*)LT_VALUE_POINTER_VALUE(LT_car(cursor)) == used_package){
            return 1;
        }
        cursor = LT_cdr(cursor);
    }

    if (cursor != LT_NIL){
        LT_error("Package used-packages must be proper list");
    }
    return 0;
}

static void package_init(LT_Package* package, char* name){
    package->base.klass = &LT_Package_class;
    package->name = LT_strdup(name);
    LT_InlineHash_init(&package->symbol_table);
    package->used_packages = LT_NIL;
    LT_InlineHash_init(&package->used_package_nicknames);
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

    package_init(&LT_Package_LISTTALK_IMPLEMENTATION, "ListTalk-implementation");
    LT_StringHash_at_put(
        package_table,
        LT_Package_LISTTALK_IMPLEMENTATION.name,
        &LT_Package_LISTTALK_IMPLEMENTATION
    );

    package_init(&LT_Package_LISTTALK_USER, "ListTalk-user");
    LT_StringHash_at_put(
        package_table,
        LT_Package_LISTTALK_USER.name,
        &LT_Package_LISTTALK_USER
    );

    package_init(&LT_Package_KEYWORD, "keyword");
    LT_StringHash_at_put(
        package_table,
        LT_Package_KEYWORD.name,
        &LT_Package_KEYWORD
    );

    LT_Package_use_package(
        &LT_Package_LISTTALK,
        &LT_Package_LISTTALK_IMPLEMENTATION,
        NULL
    );
    LT_Package_use_package(
        &LT_Package_LISTTALK_USER,
        &LT_Package_LISTTALK,
        NULL
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
    LT_Package_use_package(package, LT_PACKAGE_LISTTALK, NULL);

    return package;
}

LT_Package* LT_Package_find(char* name){
    LT_InlineHash* package_table;

    if (name == NULL){
        LT_error("Package name must not be NULL");
    }

    ensure_predefined_packages_initialized();
    package_table = get_package_table();
    return LT_StringHash_at(package_table, name);
}

char* LT_Package_name(LT_Package* package){
    ensure_predefined_packages_initialized();
    return package->name;
}

LT_Value LT_Package_used_packages(LT_Package* package){
    ensure_predefined_packages_initialized();
    return package->used_packages;
}

void LT_Package_use_package(LT_Package* package,
                            LT_Package* used_package,
                            char* nickname){
    LT_Package* nickname_package;

    ensure_predefined_packages_initialized();
    if (package == NULL || used_package == NULL){
        LT_error("use-package expects non-NULL package arguments");
    }

    if (nickname == NULL && !LT_Package_uses_package(package, used_package)){
        package->used_packages = LT_cons(
            (LT_Value)(uintptr_t)used_package,
            package->used_packages
        );
    }

    if (nickname == NULL){
        return;
    }
    if (nickname[0] == '\0'){
        LT_error("use-package nickname must not be empty");
    }

    nickname_package = LT_StringHash_at(&package->used_package_nicknames, nickname);
    if (nickname_package != NULL && nickname_package != used_package){
        LT_error("use-package nickname already bound to different package");
    }
    LT_StringHash_at_put(
        &package->used_package_nicknames,
        nickname,
        used_package
    );
}

LT_Package* LT_Package_resolve_used_package(LT_Package* package, char* name){
    LT_Package* by_nickname;
    LT_Value cursor;

    ensure_predefined_packages_initialized();
    if (package == NULL || name == NULL){
        LT_error("Package resolve expects non-NULL arguments");
    }

    by_nickname = LT_StringHash_at(&package->used_package_nicknames, name);
    if (by_nickname != NULL){
        return by_nickname;
    }

    cursor = package->used_packages;
    while (LT_Pair_p(cursor)){
        LT_Package* used_package = (LT_Package*)LT_VALUE_POINTER_VALUE(LT_car(cursor));
        if (strcmp(LT_Package_name(used_package), name) == 0){
            return used_package;
        }
        cursor = LT_cdr(cursor);
    }
    if (cursor != LT_NIL){
        LT_error("Package used-packages must be proper list");
    }

    return NULL;
}

LT_Value LT_Package_intern_local_symbol(LT_Package* package, char* name){
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
        LT_Symbol_name(LT_Symbol_from_value(value)),
        LT_VALUE_POINTER_VALUE(value)
    );
    return value;
}

LT_Value LT_Package_lookup_local_symbol(LT_Package* package, char* name){
    LT_Symbol* symbol;

    ensure_predefined_packages_initialized();
    if (package == NULL){
        LT_error("Symbol package must not be NULL");
    }
    if (name == NULL){
        LT_error("Symbol name must not be NULL");
    }

    symbol = LT_StringHash_at(&package->symbol_table, name);
    if (symbol == NULL){
        return LT_INVALID;
    }
    return ((LT_Value)(uintptr_t)symbol) | LT_VALUE_POINTER_TAG_SYMBOL;
}

LT_Value LT_Package_intern_symbol(LT_Package* package, char* name){
    LT_Symbol* symbol;
    LT_Value cursor;
    LT_Value found = LT_INVALID;

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

    cursor = package->used_packages;
    while (LT_Pair_p(cursor)){
        LT_Package* used_package = (LT_Package*)LT_VALUE_POINTER_VALUE(LT_car(cursor));

        symbol = LT_StringHash_at(&used_package->symbol_table, name);
        if (symbol != NULL){
            LT_Value used_value = ((LT_Value)(uintptr_t)symbol) | LT_VALUE_POINTER_TAG_SYMBOL;
            if (found != LT_INVALID && found != used_value){
                LT_error("Ambiguous symbol in used packages");
            }
            found = used_value;
        }
        cursor = LT_cdr(cursor);
    }
    if (cursor != LT_NIL){
        LT_error("Package used-packages must be proper list");
    }
    if (found != LT_INVALID){
        return found;
    }

    return LT_Package_intern_local_symbol(package, name);
}

LT_Package* LT_get_current_package(void){
    ensure_predefined_packages_initialized();
    if (current_package == NULL){
        current_package = LT_PACKAGE_LISTTALK;
    }
    return current_package;
}

void LT_set_current_package(LT_Package* package){
    ensure_predefined_packages_initialized();
    if (package == NULL){
        LT_error("Current package must not be NULL");
    }
    current_package = package;
}
