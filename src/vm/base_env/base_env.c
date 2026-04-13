/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/ListTalk.h>

#include <ListTalk/classes/Object.h>
#include <ListTalk/classes/Boolean.h>
#include <ListTalk/classes/Nil.h>
#include <ListTalk/classes/Character.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/ComplexNumber.h>
#include <ListTalk/classes/RealNumber.h>
#include <ListTalk/classes/RationalNumber.h>
#include <ListTalk/classes/Integer.h>
#include <ListTalk/classes/InexactComplexNumber.h>
#include <ListTalk/classes/ExactComplexNumber.h>
#include <ListTalk/classes/BigInteger.h>
#include <ListTalk/classes/Fraction.h>
#include <ListTalk/classes/Float.h>
#include <ListTalk/classes/SmallFraction.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/List.h>
#include <ListTalk/classes/ImmutableList.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Vector.h>
#include <ListTalk/classes/Closure.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/InvocationContextKind.h>
#include <ListTalk/classes/Macro.h>
#include <ListTalk/classes/SpecialForm.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/classes/Package.h>
#include <ListTalk/classes/Reader.h>
#include <ListTalk/classes/Printer.h>
#include <ListTalk/classes/IdentityDictionary.h>
#include <ListTalk/classes/Dictionary.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/Environment.h>
#include <ListTalk/utils.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern unsigned char LT_runtime_init_source[];
extern size_t LT_runtime_init_source_length;

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
    {"ComplexNumber", &LT_ComplexNumber_class},
    {"RealNumber", &LT_RealNumber_class},
    {"RationalNumber", &LT_RationalNumber_class},
    {"Integer", &LT_Integer_class},
    {"InexactComplexNumber", &LT_InexactComplexNumber_class},
    {"ExactComplexNumber", &LT_ExactComplexNumber_class},
    {"BigInteger", &LT_BigInteger_class},
    {"Fraction", &LT_Fraction_class},
    {"SmallInteger", &LT_SmallInteger_class},
    {"SmallFraction", &LT_SmallFraction_class},
    {"Float", &LT_Float_class},
    {"List", &LT_List_class},
    {"ImmutableList", &LT_ImmutableList_class},
    {"Pair", &LT_Pair_class},
    {"MutablePair", &LT_MutablePair_class},
    {"Vector", &LT_Vector_class},
    {"String", &LT_String_class},
    {"Symbol", &LT_Symbol_class},
    {"Package", &LT_Package_class},
    {"Closure", &LT_Closure_class},
    {"Primitive", &LT_Primitive_class},
    {"InvocationContextKind", &LT_InvocationContextKind_class},
    {"Macro", &LT_Macro_class},
    {"SpecialForm", &LT_SpecialForm_class},
    {"Reader", &LT_Reader_class},
    {"Printer", &LT_Printer_class},
    {"IdentityDictionary", &LT_IdentityDictionary_class},
    {"Dictionary", &LT_Dictionary_class},
};

static LT_Value modules_symbol(void){
    return LT_Symbol_new_in(LT_PACKAGE_LISTTALK, "%modules");
}

static LT_Value module_resolvers_symbol(void){
    return LT_Symbol_new_in(LT_PACKAGE_LISTTALK, "module-resolvers");
}

void LT_base_env_bind_static_primitive(LT_Environment* environment,
                                       LT_Primitive* primitive){
    LT_base_env_bind_static_primitive_in(
        environment,
        LT_PACKAGE_LISTTALK,
        primitive
    );
}

void LT_base_env_bind_static_primitive_in(LT_Environment* environment,
                                          LT_Package* package,
                                          LT_Primitive* primitive){
    LT_Value primitive_value = LT_Primitive_from_static(primitive);

    LT_Environment_bind(
        environment,
        LT_Symbol_new_in(package, primitive->name),
        primitive_value,
        LT_ENV_BINDING_FLAG_CONSTANT
    );
}

void LT_base_env_bind_native_classes(LT_Environment* environment){
    size_t i;

    for (i = 0; i < sizeof(native_class_bindings) / sizeof(native_class_bindings[0]); i++){
        LT_Value class_value = (LT_Value)(uintptr_t)native_class_bindings[i].klass;
        char* name = (char*)native_class_bindings[i].name;

        LT_Environment_bind(
            environment,
            LT_Symbol_new_in(LT_PACKAGE_LISTTALK, name),
            class_value,
            LT_ENV_BINDING_FLAG_CONSTANT
        );
    }
}

static void LT_base_env_bind_module_variables(LT_Environment* environment){
    LT_Environment_bind(
        environment,
        modules_symbol(),
        LT_NIL,
        0
    );
    LT_Environment_bind(
        environment,
        module_resolvers_symbol(),
        LT_cons((LT_Value)(uintptr_t)LT_String_new_cstr("."), LT_NIL),
        0
    );
}

void LT_base_environment_prepend_module_resolver(LT_Environment* environment,
                                                 char* resolver){
    LT_Value symbol = module_resolvers_symbol();
    LT_Value resolvers;

    if (!LT_Environment_lookup(environment, symbol, &resolvers, NULL)){
        LT_error("Module resolver variable is not bound");
    }

    if (!LT_Environment_set(
        environment,
        symbol,
        LT_cons((LT_Value)(uintptr_t)LT_String_new_cstr(resolver), resolvers)
    )){
        LT_error("Unable to update module resolver variable");
    }
}

void LT_base_environment_append_module_resolver(LT_Environment* environment,
                                                char* resolver){
    LT_Value symbol = module_resolvers_symbol();
    LT_Value resolvers;
    LT_Value cursor;
    LT_ListBuilder* builder;

    if (!LT_Environment_lookup(environment, symbol, &resolvers, NULL)){
        LT_error("Module resolver variable is not bound");
    }

    builder = LT_ListBuilder_new();
    cursor = resolvers;
    while (cursor != LT_NIL){
        if (!LT_Pair_p(cursor)){
            LT_error("Module resolver list must be proper");
        }
        LT_ListBuilder_append(builder, LT_car(cursor));
        cursor = LT_cdr(cursor);
    }

    if (!LT_Environment_set(
        environment,
        symbol,
        LT_ListBuilder_valueWithRest(
            builder,
            LT_cons((LT_Value)(uintptr_t)LT_String_new_cstr(resolver), LT_NIL)
        )
    )){
        LT_error("Unable to update module resolver variable");
    }
}

LT_Environment* LT_new_base_environment(void){
    LT_Environment* environment = LT_Environment_new(NULL, LT_NIL, LT_NIL);
    char* runtime_init_source;
    LT_Package* previous_package = LT_get_current_package();

    LT_WITH_PACKAGE(LT_PACKAGE_LISTTALK, {
        LT_base_env_bind_native_classes(environment);
        LT_base_env_bind_numbers(environment);
        LT_base_env_bind_primitives(environment);
        LT_base_env_bind_lists(environment);
        LT_base_env_bind_cxxxr(environment);
        LT_base_env_bind_strings(environment);
        LT_base_env_bind_vectors(environment);
        LT_base_env_bind_special_forms(environment);
        LT_base_env_bind_loader(environment);
        LT_base_env_bind_module_variables(environment);
        runtime_init_source = GC_MALLOC_ATOMIC(LT_runtime_init_source_length + 1);
        memcpy(runtime_init_source, LT_runtime_init_source, LT_runtime_init_source_length);
        runtime_init_source[LT_runtime_init_source_length] = '\0';
        LT_eval_sequence_string(runtime_init_source, environment);
    });
    LT_set_current_package(previous_package);

    return environment;
}

LT_Environment* LT_get_shared_base_environment(void){
    static LT_Environment* shared = NULL;

    if (shared == NULL){
        shared = LT_new_base_environment();
    }

    return shared;
}
