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
LT_String* LT_String_append(LT_String* left, LT_String* right);
LT_String* LT_String_replace(
    LT_String* string,
    LT_String* needle,
    LT_String* replacement
);
LT_String* LT_String_replaceFirst(
    LT_String* string,
    LT_String* needle,
    LT_String* replacement
);
LT_String* LT_String_mapCharacters(LT_String* string, LT_Value dictionary);
int LT_String_contains(LT_String* string, LT_String* needle);
int LT_String_find(LT_String* string, LT_String* needle, size_t* index_out);
LT_Value LT_String_findAll(LT_String* string, LT_String* needle);
LT_String* LT_String_substring(LT_String* string, size_t from, size_t to);
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
