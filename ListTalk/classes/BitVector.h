/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__BitVector__
#define H__ListTalk__BitVector__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_BitVector);
LT_DECLARE_CLASS(LT_BitVectorIterator);

LT_BitVector* LT_BitVector_new(size_t length, int fill);
size_t LT_BitVector_length(LT_BitVector* bitvector);
int LT_BitVector_at(LT_BitVector* bitvector, size_t index);
void LT_BitVector_atPut(LT_BitVector* bitvector, size_t index, int value);

LT__END_DECLS

#endif
