/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__utils__utf8__
#define H__ListTalk__utils__utf8__

#include <ListTalk/macros/env_macros.h>

#include <stdint.h>
#include <stddef.h>

LT__BEGIN_DECLS

#define LT_UTF8_REPLACEMENT_CODEPOINT UINT32_C(0xfffd)

static inline int LT_utf8_is_continuation(unsigned char ch){
    return (ch & 0xc0) == 0x80;
}

size_t LT_utf8_sequence_length(unsigned char first);
int LT_utf8_sequence_valid(const unsigned char* cursor, size_t length);
int LT_utf8_sequence_valid_bounded(const unsigned char* cursor,
                                   size_t available,
                                   size_t length);
uint32_t LT_utf8_decode_valid(const unsigned char* cursor, size_t length);
uint32_t LT_utf8_codepoint_at(const char* cursor);
uint32_t LT_utf8_codepoint_at_bounded(const char* cursor, size_t available);
const char* LT_utf8_next(const char* cursor);
const char* LT_utf8_next_bounded(const char* cursor, size_t available);
size_t LT_utf8_encode(uint32_t codepoint, char buffer[4]);

LT__END_DECLS

#endif
