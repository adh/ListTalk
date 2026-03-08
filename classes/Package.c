/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Package.h>
#include <ListTalk/vm/Class.h>

#include <ListTalk/utils.h>

struct LT_Package_s {
    LT_Object base;
    char* name;
};

static void Package_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Package* package = LT_Package_from_object(obj);
    if (package->name == NULL){
        fputs("<Package>", stream);
        return;
    }
    fputs("<Package ", stream);
    fputs(package->name, stream);
    fputc('>', stream);
}

LT_DEFINE_CLASS(LT_Package) {
    .superclass = &LT_Class_class,
    .metaclass_superclass = &LT_Class_class,
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

LT_Package* LT_Package_new(char* name){
    LT_InlineHash* package_table;
    LT_Package* package;

    if (name == NULL){
        LT_error("Package name must not be NULL");
    }

    package_table = get_package_table();
    package = LT_StringHash_at(package_table, name);
    if (package != NULL){
        return package;
    }

    package = LT_Class_ALLOC(LT_Package);
    package->name = LT_strdup(name);
    LT_StringHash_at_put(package_table, package->name, package);

    return package;
}

char* LT_Package_name(LT_Package* package){
    return package->name;
}
