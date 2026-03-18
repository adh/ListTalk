/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__List__
#define H__ListTalk__List__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

#include <stdio.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_List);

size_t LT_List_hash(LT_Value value);
int LT_List_equal_p(LT_Value left, LT_Value right);
void LT_List_debugPrintOn(LT_Value value, FILE* stream);

LT__END_DECLS

#endif
