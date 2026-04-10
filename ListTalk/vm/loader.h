/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__loader__
#define H__ListTalk__loader__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/vm/Environment.h>
#include <ListTalk/vm/value.h>

LT__BEGIN_DECLS

int LT_loader_load_file(
    char* path,
    LT_Environment* target_environment,
    LT_Value* result_out
);

LT_Value LT_loader_load(
    LT_Environment* target_environment,
    LT_Value module_designator,
    LT_Value resolvers
);

LT__END_DECLS

#endif
