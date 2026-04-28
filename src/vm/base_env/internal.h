/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__base_env_internal__
#define H__ListTalk__base_env_internal__

#include <ListTalk/vm/base_env.h>

#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Package.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/SpecialForm.h>
#include <ListTalk/vm/error.h>

static inline size_t checked_nonnegative_from_fixnum(int64_t value){
    return LT_Number_nonnegative_size_from_int64(
        value,
        "Negative index",
        "Index out of supported range"
    );
}

void LT_base_env_bind_static_primitive(LT_Environment* environment,
                                       LT_Primitive* primitive);
void LT_base_env_bind_static_primitive_in(LT_Environment* environment,
                                          LT_Package* package,
                                          LT_Primitive* primitive);
LT_Value LT_eval_argument_list(LT_Value arguments, LT_Environment* environment);

void LT_base_env_bind_numbers(LT_Environment* environment);
void LT_base_env_bind_native_classes(LT_Environment* environment);
void LT_base_env_bind_primitives(LT_Environment* environment);
void LT_base_env_bind_lists(LT_Environment* environment);
void LT_base_env_bind_cxxxr(LT_Environment* environment);
void LT_base_env_bind_strings(LT_Environment* environment);
void LT_base_env_bind_vectors(LT_Environment* environment);
void LT_base_env_bind_bytevectors(LT_Environment* environment);
void LT_base_env_bind_special_forms(LT_Environment* environment);
void LT_base_env_bind_loader(LT_Environment* environment);

#endif
