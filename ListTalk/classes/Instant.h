/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Instant__
#define H__ListTalk__Instant__

#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Instant)

LT_Value LT_Instant_new(LT_Value microseconds);
LT_Value LT_Instant_microseconds(LT_Value value);

LT__END_DECLS

#endif
