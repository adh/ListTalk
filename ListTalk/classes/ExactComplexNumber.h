/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__ExactComplexNumber__
#define H__ListTalk__ExactComplexNumber__

#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_ExactComplexNumber)

LT_Value LT_ExactComplexNumber_new(LT_Value real, LT_Value imaginary);
LT_Value LT_ExactComplexNumber_real(LT_Value value);
LT_Value LT_ExactComplexNumber_imaginary(LT_Value value);

LT__END_DECLS

#endif
