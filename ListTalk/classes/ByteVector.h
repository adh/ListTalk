/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__ByteVector__
#define H__ListTalk__ByteVector__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/classes/String.h>
#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_ByteVector);

LT_ByteVector* LT_ByteVector_new(const uint8_t* bytes, size_t length);
LT_ByteVector* LT_ByteVector_new_filled(size_t length, uint8_t fill);
LT_ByteVector* LT_ByteVector_from_string(LT_String* string);
LT_String* LT_ByteVector_to_string(LT_ByteVector* bytevector);
LT_ByteVector* LT_ByteVector_append(LT_ByteVector* left,
                                    LT_ByteVector* right);
LT_ByteVector* LT_ByteVector_from_to(LT_ByteVector* bytevector,
                                     size_t from,
                                     size_t to);
size_t LT_ByteVector_length(LT_ByteVector* bytevector);
uint8_t LT_ByteVector_at(LT_ByteVector* bytevector, size_t index);
void LT_ByteVector_atPut(LT_ByteVector* bytevector,
                         size_t index,
                         uint8_t value);
const uint8_t* LT_ByteVector_bytes(LT_ByteVector* bytevector);

LT__END_DECLS

#endif
