/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__base_env__
#define H__ListTalk__base_env__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/vm/Environment.h>

LT__BEGIN_DECLS

LT_Environment* LT_new_base_environment(void);
LT_Environment* LT_get_shared_base_environment(void);
void LT_base_environment_prepend_module_resolver(
    LT_Environment* environment,
    char* resolver
);

LT__END_DECLS

#endif
