/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__compiler__
#define H__ListTalk__compiler__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/vm/Environment.h>

LT__BEGIN_DECLS

LT_Value LT_compiler_macroexpand(
    LT_Value expression,
    LT_Environment* lexical_environment
);

LT_Value LT_compiler_fold_expression(
    LT_Value expression,
    LT_Environment* lexical_environment
);

LT__END_DECLS

#endif
