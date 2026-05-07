/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__IdentitySet__
#define H__ListTalk__IdentitySet__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/classes/Set.h>
#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_IdentitySet);
typedef struct LT_WeakIdentitySet_s LT_WeakIdentitySet;

LT_IdentitySet* LT_IdentitySet_new(void);
LT_WeakIdentitySet* LT_WeakIdentitySet_new(void);
LT_IdentitySet* LT_IdentitySet_fromList(LT_Value list);
LT_WeakIdentitySet* LT_WeakIdentitySet_fromList(LT_Value list);

LT__END_DECLS

#endif
