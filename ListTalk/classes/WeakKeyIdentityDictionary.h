/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__WeakKeyIdentityDictionary__
#define H__ListTalk__WeakKeyIdentityDictionary__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/vm/value.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_WeakKeyIdentityDictionary);

LT_WeakKeyIdentityDictionary* LT_WeakKeyIdentityDictionary_new(void);

LT__END_DECLS

#endif
