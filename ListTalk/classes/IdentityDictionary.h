/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__IdentityDictionary__
#define H__ListTalk__IdentityDictionary__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_IdentityDictionary);

LT_IdentityDictionary* LT_IdentityDictionary_new(void);
size_t LT_IdentityDictionary_size(LT_IdentityDictionary* dictionary);
void LT_IdentityDictionary_atPut(
    LT_IdentityDictionary* dictionary,
    LT_Value key,
    LT_Value value
);
int LT_IdentityDictionary_at(
    LT_IdentityDictionary* dictionary,
    LT_Value key,
    LT_Value* value_out
);
int LT_IdentityDictionary_remove(
    LT_IdentityDictionary* dictionary,
    LT_Value key,
    LT_Value* value_out
);

LT__END_DECLS

#endif
