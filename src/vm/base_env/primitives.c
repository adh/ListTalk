/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/macros/arg_macros.h>

LT_DEFINE_PRIMITIVE(
    primitive_type_of,
    "type-of",
    "(value)",
    "Return class descriptor of value."
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_Value_class(value);
}

void LT_base_env_bind_primitives(LT_Environment* environment){
    LT_base_env_bind_static_primitive(environment, &primitive_type_of);
}
