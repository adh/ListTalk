/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__BigInteger__
#define H__ListTalk__BigInteger__

#include <ListTalk/macros/decl_macros.h>

#include <stddef.h>
#include <stdint.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_BigInteger)

LT_Value LT_BigInteger_new_from_limbs(int negative,
                                      size_t limb_count,
                                      const uint32_t* limbs);
LT_Value LT_BigInteger_new_from_digits(const char* digits, unsigned int radix);
char* LT_BigInteger_to_decimal_cstr(LT_Value value);

LT__END_DECLS

#endif
