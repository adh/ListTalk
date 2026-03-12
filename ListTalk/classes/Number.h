/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Number__
#define H__ListTalk__Number__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/vm/Class.h>

#include <stdbool.h>

LT__BEGIN_DECLS

typedef struct LT_Number_s LT_Number;

extern LT_Class LT_Number_class;
extern LT_Class LT_Number_class_class;

LT_Value LT_Number_add2(LT_Value left, LT_Value right);
LT_Value LT_Number_subtract2(LT_Value left, LT_Value right);
LT_Value LT_Number_multiply2(LT_Value left, LT_Value right);
LT_Value LT_Number_divide2(LT_Value left, LT_Value right);
LT_Value LT_Number_negate(LT_Value value);
bool LT_Number_equal_p(LT_Value left, LT_Value right);

LT__END_DECLS

#endif
