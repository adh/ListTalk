/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Set__
#define H__ListTalk__Set__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

#include <stddef.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Set);

LT_Set* LT_Set_new(void);
LT_Set* LT_Set_fromList(LT_Value list);
size_t LT_Set_size(LT_Set* set);
int LT_Set_put(LT_Set* set, LT_Value value);
int LT_Set_contains(LT_Set* set, LT_Value value);
LT_Value LT_Set_asList(LT_Set* set);
void LT_Set_for_each(LT_Set* set, LT_Value callable);
LT_Value LT_Set_any(LT_Set* set, LT_Value callable);
LT_Value LT_Set_every(LT_Set* set, LT_Value callable);
LT_Value LT_Set_inject_into(LT_Set* set, LT_Value initial, LT_Value callable);
LT_Value LT_Set_reduce(LT_Set* set, LT_Value callable);

LT__END_DECLS

#endif
