/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__String__
#define H__ListTalk__String__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

#include <stdint.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_String);

LT_String* LT_String_new(char* buf, size_t len);
LT_String* LT_String_new_cstr(char* str);
const char* LT_String_value_cstr(LT_String* string);
size_t LT_String_length(LT_String* string);
size_t LT_String_byte_length(LT_String* string);
uint32_t LT_String_utf8_codepoint_at(const char* cursor);
const char* LT_String_utf8_next(const char* cursor);
uint32_t LT_String_at(LT_String* string, size_t index);
LT_Value LT_String_to_character_list(LT_String* string);
LT_String* LT_String_from_character_list(LT_Value characters);

LT__END_DECLS

#endif
