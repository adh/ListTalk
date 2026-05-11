/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__WeakValueIdentityDictionary__
#define H__ListTalk__WeakValueIdentityDictionary__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/vm/value.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_WeakValueIdentityDictionary);

LT_WeakValueIdentityDictionary* LT_WeakValueIdentityDictionary_new(void);
LT_WeakValueIdentityDictionary* LT_WeakValueIdentityDictionary_newFromAList(
    LT_Value alist
);

LT__END_DECLS

#endif
