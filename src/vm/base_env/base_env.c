/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Symbol.h>

void LT_base_env_bind_primitive(LT_Environment* environment,
                                char* name,
                                LT_Primitive_Func function){
    LT_Environment_bind(
        environment,
        LT_Symbol_new(name),
        LT_Primitive_new(name, function),
        LT_ENV_BINDING_FLAG_CONSTANT
    );
}

LT_Environment* LT_new_base_environment(void){
    LT_Environment* environment = LT_Environment_new(NULL);

    LT_base_env_bind_numbers(environment);
    LT_base_env_bind_primitives(environment);
    LT_base_env_bind_lists(environment);
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
