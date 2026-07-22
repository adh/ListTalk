/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__XoshiroRNG__
#define H__ListTalk__XoshiroRNG__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/macros/decl_macros.h>

#include <stdint.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_XoshiroRNG);

LT_XoshiroRNG* LT_XoshiroRNG_from_seed(int64_t seed);

LT__END_DECLS

#endif
