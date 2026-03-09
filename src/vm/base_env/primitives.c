/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/macros/arg_macros.h>

static LT_Value primitive_type_of(LT_Value arguments){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_Value_class(value);
}

void LT_base_env_bind_primitives(LT_Environment* environment){
    LT_base_env_bind_primitive(environment, "type-of", primitive_type_of);
}
