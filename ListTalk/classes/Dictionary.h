/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Dictionary__
#define H__ListTalk__Dictionary__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Dictionary);

LT_Dictionary* LT_Dictionary_new(void);
size_t LT_Dictionary_size(LT_Dictionary* dictionary);
void LT_Dictionary_atPut(
    LT_Dictionary* dictionary,
    LT_Value key,
    LT_Value value
);
int LT_Dictionary_at(
    LT_Dictionary* dictionary,
    LT_Value key,
    LT_Value* value_out
);
int LT_Dictionary_remove(
    LT_Dictionary* dictionary,
    LT_Value key,
    LT_Value* value_out
);

LT__END_DECLS

#endif
