/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Package.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Class.h>
#include <ListTalk/vm/thread_state.h>
#include <ListTalk/macros/arg_macros.h>

#include <ListTalk/utils.h>
#include <string.h>

extern LT_Value LT__Symbol_new_uninterned(LT_Package* package, char* name);

struct LT_Package_s {
    LT_Object base;
    char* name;
    LT_InlineHash symbol_table;
    LT_InlineHash* exported_symbols;
    LT_Value used_packages;
    LT_InlineHash used_package_nicknames;
};

LT_Package LT_Package_LISTTALK = {0};
LT_Package LT_Package_LISTTALK_IMPLEMENTATION = {0};
LT_Package LT_Package_LISTTALK_USER = {0};
LT_Package LT_Package_KEYWORD = {0};
static LT_InlineHash package_table;
static pthread_once_t package_table_once = PTHREAD_ONCE_INIT;
static pthread_once_t predefined_packages_once = PTHREAD_ONCE_INIT;

static LT_InlineHash* get_package_table(void);
static void ensure_predefined_packages_initialized(void);

static void* package_string_table_at_locked(
    LT_InlineHash* table,
    char* name,
    size_t hash
){
    LT_InlineHash_Entry* entry = table->vector[hash & table->mask];

    while (entry != NULL){
        if (entry->hash == hash && strcmp(entry->key, name) == 0){
            return entry->value;
        }
        entry = entry->next;
    }
    return NULL;
}

static void package_string_table_grow_locked(LT_InlineHash* table){
    LT_InlineHash_Entry** grown_vector;
    size_t grown_size;
    size_t i;

    grown_size = (table->mask + 1) << 1;
    grown_vector = GC_MALLOC(sizeof(LT_InlineHash_Entry*) * grown_size);
    memset(grown_vector, 0, sizeof(LT_InlineHash_Entry*) * grown_size);

    for (i = 0; i < table->mask + 1; i++){
        LT_InlineHash_Entry* entry = table->vector[i];

        while (entry != NULL){
            LT_InlineHash_Entry* next = entry->next;
            size_t index = entry->hash & (grown_size - 1);

            entry->next = grown_vector[index];
            grown_vector[index] = entry;
            entry = next;
        }
    }

    table->vector = grown_vector;
    table->mask = grown_size - 1;
}

static void package_string_table_at_put_locked(
    LT_InlineHash* table,
    char* name,
    size_t hash,
    void* value
){
    LT_InlineHash_Entry* entry;

    if (table->count > table->mask){
        package_string_table_grow_locked(table);
    }

    entry = GC_NEW(LT_InlineHash_Entry);
    entry->hash = hash;
    entry->key = LT_strdup(name);
    entry->value = value;
    entry->next = table->vector[hash & table->mask];
    table->vector[hash & table->mask] = entry;
    table->count++;
}

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

static char* package_name_designator(LT_Value value){
    if (LT_Symbol_p(value)){
        return LT_Symbol_name(LT_Symbol_from_value(value));
    }
    if (LT_String_p(value)){
        return (char*)LT_String_value_cstr(LT_String_from_value(value));
    }
    LT_error("Package name designator must be symbol or string");
    return NULL;
}

LT_DEFINE_PRIMITIVE(
    package_class_method_named,
    "Package class>>named:",
    "(self name)",
    "Return package with the provided name, creating it when needed."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value name_designator;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, name_designator);
    LT_ARG_END(cursor);

    if (self != (LT_Value)(uintptr_t)&LT_Package_class){
        LT_error("named: class method is only supported on Package");
    }

    return (LT_Value)(uintptr_t)LT_Package_new(
        package_name_designator(name_designator)
    );
}

LT_DEFINE_PRIMITIVE(
    package_class_method_find,
    "Package class>>find:",
    "(self name)",
    "Return package with the provided name, or nil when no such package exists."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value name_designator;
    LT_Package* package;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, name_designator);
    LT_ARG_END(cursor);

    if (self != (LT_Value)(uintptr_t)&LT_Package_class){
        LT_error("find: class method is only supported on Package");
    }

    package = LT_Package_find(package_name_designator(name_designator));
    if (package == NULL){
        return LT_NIL;
    }
    return (LT_Value)(uintptr_t)package;
}

LT_DEFINE_PRIMITIVE(
    package_method_name,
    "Package>>name",
    "(self)",
    "Return package name."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_String_new_cstr(
        LT_Package_name(LT_Package_from_value(self))
    );
}

LT_Package* LT_Package_for_designator(LT_Value designator, bool create_missing){
    if (LT_Package_p(designator)){
        return LT_Package_from_value(designator);
    }

    char* name = package_name_designator(designator);
    LT_Package* package = LT_Package_find(name);

    if (package == NULL && create_missing){
        package = LT_Package_new(name);
    }
    return package;
}

void LT_Package_symbols_do(LT_Package* package, LT_Value callable){
    LT_Value symbols = LT_Package_symbols_asList(package);

    while (symbols != LT_NIL){
        (void)LT_apply(
            callable,
            LT_cons(LT_car(symbols), LT_NIL),
            LT_NIL,
            LT_NIL,
            NULL
        );
        symbols = LT_cdr(symbols);
    }
}

LT_Value LT_Package_symbols_asList(LT_Package* package){
    LT_InlineHash* table;
    LT_ListBuilder* builder = LT_ListBuilder_new();
    size_t i;

    ensure_predefined_packages_initialized();
    if (package == NULL){
        LT_error("Package symbolsAsList expects non-NULL package");
    }
    table = &package->symbol_table;
    LT_MutexWord_lock(&table->lock);
    for (i = 0; i < table->mask + 1; i++){
        LT_InlineHash_Entry* entry = table->vector[i];

        while (entry != NULL){
            LT_ListBuilder_append(
                builder,
                ((LT_Value)(uintptr_t)entry->value) | LT_VALUE_POINTER_TAG_SYMBOL
            );
            entry = entry->next;
        }
    }
    LT_MutexWord_unlock(&table->lock);
    return LT_ListBuilder_value(builder);
}

LT_Value LT_Package_exported_symbols_asList(LT_Package* package){
    LT_ListBuilder* builder;
    LT_InlineHash* table;
    size_t i;

    ensure_predefined_packages_initialized();
    if (package == NULL){
        LT_error("Package exportedSymbolsAsList expects non-NULL package");
    }
    table = package->exported_symbols;
    if (table == NULL){
        return LT_Package_symbols_asList(package);
    }

    builder = LT_ListBuilder_new();
    LT_MutexWord_lock(&table->lock);
    for (i = 0; i < table->mask + 1; i++){
        LT_InlineHash_Entry* entry = table->vector[i];

        while (entry != NULL){
            LT_ListBuilder_append(
                builder,
                ((LT_Value)(uintptr_t)entry->value) | LT_VALUE_POINTER_TAG_SYMBOL
            );
            entry = entry->next;
        }
    }
    LT_MutexWord_unlock(&table->lock);
    return LT_ListBuilder_value(builder);
}

void LT_Package_packages_do(LT_Value callable){
    LT_Value packages = LT_Package_packages_asList();

    while (packages != LT_NIL){
        (void)LT_apply(
            callable,
            LT_cons(LT_car(packages), LT_NIL),
            LT_NIL,
            LT_NIL,
            NULL
        );
        packages = LT_cdr(packages);
    }
}

LT_Value LT_Package_packages_asList(void){
    LT_InlineHash* table = get_package_table();
    LT_ListBuilder* builder = LT_ListBuilder_new();
    size_t i;

    ensure_predefined_packages_initialized();
    LT_MutexWord_lock(&table->lock);
    for (i = 0; i < table->mask + 1; i++){
        LT_InlineHash_Entry* entry = table->vector[i];

        while (entry != NULL){
            LT_ListBuilder_append(builder, (LT_Value)(uintptr_t)entry->value);
            entry = entry->next;
        }
    }
    LT_MutexWord_unlock(&table->lock);
    return LT_ListBuilder_value(builder);
}

LT_DEFINE_PRIMITIVE(
    package_method_symbols_do,
    "Package>>symbolsDo:",
    "(self callable)",
    "Call callable for each local interned symbol."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);

    LT_Package_symbols_do(LT_Package_from_value(self), callable);
    return LT_NIL;
}

LT_DEFINE_PRIMITIVE(
    package_method_symbols_as_list,
    "Package>>symbolsAsList",
    "(self)",
    "Return local interned symbols as a list."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Package_symbols_asList(LT_Package_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    package_method_exported_symbols_as_list,
    "Package>>exportedSymbolsAsList",
    "(self)",
    "Return exported symbols as a list."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Package_exported_symbols_asList(LT_Package_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    package_method_export,
    "Package>>export:",
    "(self symbol)",
    "Mark symbol as exported from package."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value symbol;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, symbol);
    LT_ARG_END(cursor);
    LT_Package_export_symbol(LT_Package_from_value(self), symbol);
    return symbol;
}

LT_DEFINE_PRIMITIVE(
    package_method_unexport,
    "Package>>unexport:",
    "(self symbol)",
    "Mark symbol as not exported from package."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value symbol;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, symbol);
    LT_ARG_END(cursor);
    LT_Package_unexport_symbol(LT_Package_from_value(self), symbol);
    return symbol;
}

LT_DEFINE_PRIMITIVE(
    package_class_method_packages_do,
    "Package class>>packagesDo:",
    "(self callable)",
    "Call callable for each package."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);

    if (self != (LT_Value)(uintptr_t)&LT_Package_class){
        LT_error("packagesDo: class method is only supported on Package");
    }

    LT_Package_packages_do(callable);
    return LT_NIL;
}

LT_DEFINE_PRIMITIVE(
    package_class_method_packages_as_list,
    "Package class>>packagesAsList",
    "(self)",
    "Return packages as a list."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    if (self != (LT_Value)(uintptr_t)&LT_Package_class){
        LT_error("packagesAsList class method is only supported on Package");
    }

    return LT_Package_packages_asList();
}

static int normalize_comparison(int comparison){
    if (comparison < 0){
        return -1;
    }
    if (comparison > 0){
        return 1;
    }
    return 0;
}

static int package_compare(LT_Package* self, LT_Package* other){
    return normalize_comparison(strcmp(LT_Package_name(self), LT_Package_name(other)));
}

LT_DEFINE_PRIMITIVE(
    package_method_compare_with,
    "Package>>compareWith:",
    "(self other)",
    "Return -1, 0, or 1 when receiver name is lexicographically less than, equal to, or greater than argument name."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, other);
    LT_ARG_END(cursor);
    return LT_SmallInteger_new(package_compare(
        LT_Package_from_value(self),
        LT_Package_from_value(other)
    ));
}

LT_DEFINE_PRIMITIVE(
    package_method_less_than,
    "Package>><",
    "(self other)",
    "Return true when receiver name is lexicographically less than argument name."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, other);
    LT_ARG_END(cursor);
    return package_compare(LT_Package_from_value(self), LT_Package_from_value(other)) < 0
        ? LT_TRUE
        : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    package_method_greater_than,
    "Package>>>",
    "(self other)",
    "Return true when receiver name is lexicographically greater than argument name."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, other);
    LT_ARG_END(cursor);
    return package_compare(LT_Package_from_value(self), LT_Package_from_value(other)) > 0
        ? LT_TRUE
        : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    package_method_less_than_or_equal,
    "Package>><=",
    "(self other)",
    "Return true when receiver name is lexicographically less than or equal to argument name."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, other);
    LT_ARG_END(cursor);
    return package_compare(LT_Package_from_value(self), LT_Package_from_value(other)) <= 0
        ? LT_TRUE
        : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    package_method_greater_than_or_equal,
    "Package>>>=",
    "(self other)",
    "Return true when receiver name is lexicographically greater than or equal to argument name."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, other);
    LT_ARG_END(cursor);
    return package_compare(LT_Package_from_value(self), LT_Package_from_value(other)) >= 0
        ? LT_TRUE
        : LT_FALSE;
}

static LT_Method_Descriptor Package_methods[] = {
    {"name", &package_method_name},
    {"symbolsDo:", &package_method_symbols_do},
    {"symbolsAsList", &package_method_symbols_as_list},
    {"exportedSymbolsAsList", &package_method_exported_symbols_as_list},
    {"export:", &package_method_export},
    {"unexport:", &package_method_unexport},
    {"compareWith:", &package_method_compare_with},
    {"<", &package_method_less_than},
    {">", &package_method_greater_than},
    {"<=", &package_method_less_than_or_equal},
    {">=", &package_method_greater_than_or_equal},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor Package_class_methods[] = {
    {"named:", &package_class_method_named},
    {"find:", &package_class_method_find},
    {"packagesDo:", &package_class_method_packages_do},
    {"packagesAsList", &package_class_method_packages_as_list},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Package) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Package",
    .documentation = "Namespace for interned symbols.",
    .instance_size = sizeof(LT_Package),
    .debugPrintOn = Package_debugPrintOn,
    .methods = Package_methods,
    .class_methods = Package_class_methods,
};

static void package_table_init_once(void){
    LT_InlineHash_init(&package_table);
}

static LT_InlineHash* get_package_table(void){
    pthread_once(&package_table_once, package_table_init_once);

    return &package_table;
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
    package->exported_symbols = NULL;
    package->used_packages = LT_NIL;
    LT_InlineHash_init(&package->used_package_nicknames);
}

static void package_use_package_initialized(
    LT_Package* package,
    LT_Package* used_package,
    char* nickname
){
    LT_Package* nickname_package;

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

static void predefined_packages_init_once(void){
    LT_InlineHash* package_table = get_package_table();

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

    package_use_package_initialized(
        &LT_Package_LISTTALK,
        &LT_Package_LISTTALK_IMPLEMENTATION,
        NULL
    );
    package_use_package_initialized(
        &LT_Package_LISTTALK_USER,
        &LT_Package_LISTTALK,
        NULL
    );
}

static void ensure_predefined_packages_initialized(void){
    pthread_once(&predefined_packages_once, predefined_packages_init_once);
}

static void LT___init_predefined_packages(void){
    ensure_predefined_packages_initialized();
}
LT_REGISTER_INITIALIZER(LT___init_predefined_packages)

LT_Package* LT_Package_new(char* name){
    LT_InlineHash* package_table;
    LT_Package* package;
    size_t hash;

    if (name == NULL){
        LT_error("Package name must not be NULL");
    }

    ensure_predefined_packages_initialized();
    package_table = get_package_table();
    hash = LT_fnv_hash(name);

    LT_MutexWord_lock(&package_table->lock);
    package = package_string_table_at_locked(package_table, name, hash);
    if (package != NULL){
        LT_MutexWord_unlock(&package_table->lock);
        return package;
    }

    package = LT_Class_ALLOC(LT_Package);
    package_init(package, name);
    package_string_table_at_put_locked(
        package_table,
        package->name,
        hash,
        package
    );
    LT_MutexWord_unlock(&package_table->lock);
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
    ensure_predefined_packages_initialized();
    if (package == NULL || used_package == NULL){
        LT_error("use-package expects non-NULL package arguments");
    }
    package_use_package_initialized(package, used_package, nickname);
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
    LT_InlineHash* table;
    LT_Symbol* symbol;
    LT_Value value;
    size_t hash;

    ensure_predefined_packages_initialized();
    if (package == NULL){
        LT_error("Symbol package must not be NULL");
    }
    if (name == NULL){
        LT_error("Symbol name must not be NULL");
    }

    table = &package->symbol_table;
    hash = LT_fnv_hash(name);

    LT_MutexWord_lock(&table->lock);
    symbol = (LT_Symbol*)package_string_table_at_locked(table, name, hash);
    if (symbol != NULL){
        LT_MutexWord_unlock(&table->lock);
        return ((LT_Value)(uintptr_t)symbol) | LT_VALUE_POINTER_TAG_SYMBOL;
    }

    value = LT__Symbol_new_uninterned(package, name);
    package_string_table_at_put_locked(
        table,
        LT_Symbol_name(LT_Symbol_from_value(value)),
        hash,
        LT_VALUE_POINTER_VALUE(value)
    );
    LT_MutexWord_unlock(&table->lock);
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

static LT_InlineHash* package_ensure_exported_symbols_table(
    LT_Package* package,
    int include_current_symbols
){
    if (package->exported_symbols == NULL){
        LT_InlineHash* table = GC_NEW(LT_InlineHash);

        LT_InlineHash_init(table);
        package->exported_symbols = table;
        if (include_current_symbols){
            LT_Value cursor = LT_Package_symbols_asList(package);

            while (LT_Pair_p(cursor)){
                LT_Value symbol = LT_car(cursor);
                LT_Symbol* symbol_object = LT_Symbol_from_value(symbol);

                LT_StringHash_at_put(
                    table,
                    LT_Symbol_name(symbol_object),
                    (void*)LT_VALUE_POINTER_VALUE(symbol)
                );
                cursor = LT_cdr(cursor);
            }
            if (cursor != LT_NIL){
                LT_error("Package symbols must be proper list");
            }
        }
    }
    return package->exported_symbols;
}

int LT_Package_symbol_exported_p(LT_Package* package, LT_Value symbol){
    LT_Symbol* symbol_object;
    LT_Value local_symbol;
    LT_Symbol* exported_symbol;

    ensure_predefined_packages_initialized();
    if (package == NULL){
        LT_error("Package symbol export check expects non-NULL package");
    }
    if (!LT_Symbol_p(symbol)){
        LT_type_error(symbol, &LT_Symbol_class);
    }

    symbol_object = LT_Symbol_from_value(symbol);
    if (package->exported_symbols != NULL){
        exported_symbol = LT_StringHash_at(
            package->exported_symbols,
            LT_Symbol_name(symbol_object)
        );
        return exported_symbol == (LT_Symbol*)LT_VALUE_POINTER_VALUE(symbol);
    }

    if (LT_Symbol_package(symbol_object) != package){
        return 0;
    }
    local_symbol = LT_Package_lookup_local_symbol(
        package,
        LT_Symbol_name(symbol_object)
    );
    return local_symbol == symbol;
}

LT_Value LT_Package_lookup_exported_symbol(LT_Package* package, char* name){
    LT_Value found = LT_INVALID;

    ensure_predefined_packages_initialized();
    if (package == NULL){
        LT_error("Package exported lookup expects non-NULL package");
    }
    if (name == NULL){
        LT_error("Symbol name must not be NULL");
    }

    if (package->exported_symbols == NULL){
        return LT_Package_lookup_local_symbol(package, name);
    }

    {
        LT_Symbol* symbol = LT_StringHash_at(package->exported_symbols, name);

        if (symbol != NULL){
            found = ((LT_Value)(uintptr_t)symbol) | LT_VALUE_POINTER_TAG_SYMBOL;
        }
    }
    return found;
}

void LT_Package_export_symbol(LT_Package* package, LT_Value symbol){
    LT_InlineHash* table;
    LT_Symbol* symbol_object;
    LT_Symbol* exported_symbol;

    ensure_predefined_packages_initialized();
    if (package == NULL){
        LT_error("Package export expects non-NULL package");
    }
    if (!LT_Symbol_p(symbol)){
        LT_type_error(symbol, &LT_Symbol_class);
    }

    table = package_ensure_exported_symbols_table(package, 0);
    symbol_object = LT_Symbol_from_value(symbol);
    {
        char* key = LT_Symbol_name(symbol_object);
        size_t hash = LT_fnv_hash(key);

        LT_MutexWord_lock(&table->lock);
        exported_symbol = (LT_Symbol*)package_string_table_at_locked(
            table,
            key,
            hash
        );
        if (exported_symbol != NULL){
            LT_MutexWord_unlock(&table->lock);
            if (exported_symbol != (LT_Symbol*)LT_VALUE_POINTER_VALUE(symbol)){
                LT_error("Exported symbol name already bound to different symbol");
            }
            return;
        }
        package_string_table_at_put_locked(
            table,
            key,
            hash,
            (void*)LT_VALUE_POINTER_VALUE(symbol)
        );
        LT_MutexWord_unlock(&table->lock);
    }
}

void LT_Package_unexport_symbol(LT_Package* package, LT_Value symbol){
    LT_InlineHash* table;
    LT_Symbol* symbol_object;
    void* removed;

    ensure_predefined_packages_initialized();
    if (package == NULL){
        LT_error("Package unexport expects non-NULL package");
    }
    if (!LT_Symbol_p(symbol)){
        LT_type_error(symbol, &LT_Symbol_class);
    }

    table = package_ensure_exported_symbols_table(package, 1);
    symbol_object = LT_Symbol_from_value(symbol);
    if (LT_StringHash_at(table, LT_Symbol_name(symbol_object))
        != (LT_Symbol*)LT_VALUE_POINTER_VALUE(symbol)){
        return;
    }
    (void)LT_StringHash_remove(
        table,
        LT_Symbol_name(symbol_object),
        &removed
    );
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

        LT_Value used_value = LT_Package_lookup_exported_symbol(used_package, name);
        if (used_value != LT_INVALID){
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
    LT_ThreadState* state = LT_thread_state();

    ensure_predefined_packages_initialized();
    if (!state->current_package_is_set){
        state->current_package = LT_PACKAGE_LISTTALK;
        state->current_package_is_set = 1;
    }
    return state->current_package;
}

void LT_set_current_package(LT_Package* package){
    LT_ThreadState* state = LT_thread_state();

    ensure_predefined_packages_initialized();
    state->current_package = package;
    state->current_package_is_set = 1;
}
