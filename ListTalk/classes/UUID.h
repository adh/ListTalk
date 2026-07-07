/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__UUID__
#define H__ListTalk__UUID__

#include <ListTalk/classes/String.h>
#include <ListTalk/macros/decl_macros.h>

#include <stdint.h>

LT__BEGIN_DECLS

#define LT_UUID_BYTE_LENGTH 16
#define LT_UUID_STRING_LENGTH 36

LT_DECLARE_CLASS(LT_UUID);

LT_UUID* LT_UUID_new(const uint8_t bytes[LT_UUID_BYTE_LENGTH]);
LT_UUID* LT_UUID_random_v4(void);
LT_UUID* LT_UUID_from_string(LT_String* string);
LT_String* LT_UUID_as_string(LT_UUID* uuid);
int LT_UUID_compare(LT_UUID* left, LT_UUID* right);
uint8_t LT_UUID_at(LT_UUID* uuid, size_t index);
const uint8_t* LT_UUID_bytes(LT_UUID* uuid);

LT__END_DECLS

#endif
