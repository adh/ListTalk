/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Integer__
#define H__ListTalk__Integer__

#include <ListTalk/macros/decl_macros.h>

#include <stdint.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Integer)

LT_Value LT_Integer_from_intmax(intmax_t value);
LT_Value LT_Integer_from_uintmax(uintmax_t value);

LT__END_DECLS

#endif
