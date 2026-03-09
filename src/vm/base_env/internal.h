/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__base_env_internal__
#define H__ListTalk__base_env_internal__

#include <ListTalk/vm/base_env.h>

#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/SpecialForm.h>

void LT_base_env_bind_primitive(LT_Environment* environment,
                                char* name,
                                LT_Primitive_Func function);

void LT_base_env_bind_numbers(LT_Environment* environment);
void LT_base_env_bind_primitives(LT_Environment* environment);
void LT_base_env_bind_special_forms(LT_Environment* environment);

#endif
