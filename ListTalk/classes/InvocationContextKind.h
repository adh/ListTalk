/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__InvocationContextKind__
#define H__ListTalk__InvocationContextKind__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/classes/Object.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_InvocationContextKind);

struct LT_InvocationContextKind_s {
    LT_Object base;
};

LT__END_DECLS

#endif
