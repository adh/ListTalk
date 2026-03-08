/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Vector__
#define H__ListTalk__Vector__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Vector);

LT_Vector* LT_Vector_new(size_t length);
size_t LT_Vector_length(LT_Vector* vector);
LT_Value LT_Vector_at(LT_Vector* vector, size_t index);
void LT_Vector_atPut(LT_Vector* vector, size_t index, LT_Value value);

LT__END_DECLS

#endif
