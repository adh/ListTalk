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

int LT_Number_parse_token_with_radix(const char* token, unsigned int radix, LT_Value* value);
char* LT_Number_to_string(LT_Value value);
double LT_Number_to_double(LT_Value value);
LT_Value LT_Number_add2(LT_Value left, LT_Value right);
LT_Value LT_Number_subtract2(LT_Value left, LT_Value right);
LT_Value LT_Number_multiply2(LT_Value left, LT_Value right);
LT_Value LT_Number_divide2(LT_Value left, LT_Value right);
LT_Value LT_Number_negate(LT_Value value);
LT_Value LT_Number_abs(LT_Value value);
LT_Value LT_Number_phase(LT_Value value);
LT_Value LT_Number_floor(LT_Value value);
LT_Value LT_Number_truncate(LT_Value value);
LT_Value LT_Number_ceiling(LT_Value value);
LT_Value LT_Number_round(LT_Value value);
LT_Value LT_Number_sin(LT_Value value);
LT_Value LT_Number_cos(LT_Value value);
LT_Value LT_Number_tan(LT_Value value);
LT_Value LT_Number_log(LT_Value value);
LT_Value LT_Number_exp(LT_Value value);
LT_Value LT_Number_expt(LT_Value base, LT_Value exponent);
bool LT_Number_equal_p(LT_Value left, LT_Value right);
int LT_Number_compare(LT_Value left, LT_Value right);

LT__END_DECLS

#endif
