/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Integer__
#define H__ListTalk__Integer__

#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/BigInteger.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Integer)

/* Fast tag-based check: true for SmallInteger (fixnum) and BigInteger. */
static inline int LT_Integer_value_p(LT_Value value){
    return LT_Value_is_fixnum(value) || LT_BigInteger_p(value);
}

LT__END_DECLS

#endif
