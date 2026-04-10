/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/loader.h>

LT_DEFINE_PRIMITIVE(
    primitive_load_bang,
    "%load!",
    "(environment module-designator resolvers)",
    "Load module source from resolver list into environment."
){
    LT_Value cursor = arguments;
    LT_Value environment_value;
    LT_Value module_designator;
    LT_Value resolvers;
    LT_Environment* target_environment;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_OBJECT_ARG(cursor, environment_value);
    LT_OBJECT_ARG(cursor, module_designator);
    LT_OBJECT_ARG(cursor, resolvers);
    LT_ARG_END(cursor);

    target_environment = LT_Environment_from_value(environment_value);
    return LT_loader_load(target_environment, module_designator, resolvers);
}

void LT_base_env_bind_loader(LT_Environment* environment){
    LT_base_env_bind_static_primitive_in(
        environment,
        LT_PACKAGE_LISTTALK_IMPLEMENTATION,
        &primitive_load_bang
    );
}
