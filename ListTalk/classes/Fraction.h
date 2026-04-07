/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Fraction__
#define H__ListTalk__Fraction__

#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Fraction)

LT_Value LT_Fraction_numerator(LT_Value value);
LT_Value LT_Fraction_denominator(LT_Value value);

LT__END_DECLS

#endif
