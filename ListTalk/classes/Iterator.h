/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Iterator__
#define H__ListTalk__Iterator__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/vm/value.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Iterator);

LT_Value LT_Iterator_this(LT_Value iterator);
LT_Value LT_Iterator_hasThis(LT_Value iterator);
LT_Value LT_Iterator_next(LT_Value iterator);

LT__END_DECLS

#endif
