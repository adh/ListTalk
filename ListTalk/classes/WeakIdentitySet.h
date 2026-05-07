/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__WeakIdentitySet__
#define H__ListTalk__WeakIdentitySet__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/vm/value.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_WeakIdentitySet);

LT_WeakIdentitySet* LT_WeakIdentitySet_new(void);
LT_WeakIdentitySet* LT_WeakIdentitySet_fromList(LT_Value list);

LT__END_DECLS

#endif
