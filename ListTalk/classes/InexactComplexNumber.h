/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__InexactComplexNumber__
#define H__ListTalk__InexactComplexNumber__

#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_InexactComplexNumber)

LT_Value LT_InexactComplexNumber_new(double real, double imaginary);
double LT_InexactComplexNumber_real(LT_Value value);
double LT_InexactComplexNumber_imaginary(LT_Value value);

LT__END_DECLS

#endif
