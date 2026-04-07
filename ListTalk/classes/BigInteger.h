/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__BigInteger__
#define H__ListTalk__BigInteger__

#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_BigInteger)

LT_Value LT_BigInteger_new_from_digits(const char* digits);
char* LT_BigInteger_to_decimal_cstr(LT_Value value);

LT__END_DECLS

#endif
