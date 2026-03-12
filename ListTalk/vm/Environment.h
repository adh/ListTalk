/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Environment__
#define H__ListTalk__Environment__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Environment);

#define LT_ENV_BINDING_FLAG_CONSTANT 0x1

#define LT_ENV_FRAME_FLAG_RECYCLABLE 0x1

LT_Environment* LT_Environment_new(LT_Environment* parent);
LT_Environment* LT_Environment_parent(LT_Environment* environment);

void LT_Environment_bind(
    LT_Environment* environment,
    LT_Value symbol,
    LT_Value value,
    unsigned int flags
);

int LT_Environment_lookup(
    LT_Environment* environment,
    LT_Value symbol,
    LT_Value* value_out,
    unsigned int* flags_out
);

int LT_Environment_set(
    LT_Environment* environment,
    LT_Value symbol,
    LT_Value value
);

LT__END_DECLS

#endif
