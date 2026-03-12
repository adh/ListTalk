/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/classes/Object.h>
#include <ListTalk/classes/Boolean.h>
#include <ListTalk/classes/Nil.h>
#include <ListTalk/classes/Character.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Float.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Vector.h>
#include <ListTalk/classes/Closure.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Macro.h>
#include <ListTalk/classes/SpecialForm.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/classes/Package.h>
#include <ListTalk/classes/Reader.h>
#include <ListTalk/classes/Printer.h>
#include <ListTalk/classes/IdentityDictionary.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/Environment.h>

#include <stddef.h>
#include <stdint.h>

struct LT_NativeClassBinding {
    const char* name;
    LT_Class* klass;
};

static const struct LT_NativeClassBinding native_class_bindings[] = {
    {"Object", &LT_Object_class},
    {"Class", &LT_Class_class},
    {"Environment", &LT_Environment_class},
    {"Boolean", &LT_Boolean_class},
    {"Nil", &LT_Nil_class},
    {"Character", &LT_Character_class},
    {"Number", &LT_Number_class},
    {"SmallInteger", &LT_SmallInteger_class},
    {"Float", &LT_Float_class},
    {"Pair", &LT_Pair_class},
    {"Vector", &LT_Vector_class},
    {"String", &LT_String_class},
    {"Symbol", &LT_Symbol_class},
    {"Package", &LT_Package_class},
    {"Closure", &LT_Closure_class},
    {"Primitive", &LT_Primitive_class},
    {"Macro", &LT_Macro_class},
    {"SpecialForm", &LT_SpecialForm_class},
    {"Reader", &LT_Reader_class},
    {"Printer", &LT_Printer_class},
    {"IdentityDictionary", &LT_IdentityDictionary_class},
};

void LT_base_env_bind_static_primitive(LT_Environment* environment,
                                       LT_Primitive* primitive){
    LT_Environment_bind(
        environment,
        LT_Symbol_new(primitive->name),
        LT_Primitive_from_static(primitive),
        LT_ENV_BINDING_FLAG_CONSTANT
    );
}

void LT_base_env_bind_native_classes(LT_Environment* environment){
    size_t i;

    for (i = 0; i < sizeof(native_class_bindings) / sizeof(native_class_bindings[0]); i++){
        LT_Environment_bind(
            environment,
            LT_Symbol_new((char*)native_class_bindings[i].name),
            (LT_Value)(uintptr_t)native_class_bindings[i].klass,
            LT_ENV_BINDING_FLAG_CONSTANT
        );
    }
}

LT_Environment* LT_new_base_environment(void){
    LT_Environment* environment = LT_Environment_new(NULL);

    LT_base_env_bind_native_classes(environment);
    LT_base_env_bind_numbers(environment);
    LT_base_env_bind_primitives(environment);
    LT_base_env_bind_lists(environment);
    LT_base_env_bind_strings(environment);
    LT_base_env_bind_vectors(environment);
    LT_base_env_bind_special_forms(environment);

    return environment;
}

LT_Environment* LT_get_shared_base_environment(void){
    static LT_Environment* shared = NULL;

    if (shared == NULL){
        shared = LT_new_base_environment();
    }

    return shared;
}
