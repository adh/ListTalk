/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Ales Hakl
 */

#ifndef H__ListTalk__utils__unicode_data__
#define H__ListTalk__utils__unicode_data__

#include <stdint.h>
#include <stddef.h>

const char* LT_unicode_category(uint32_t codepoint);

static inline int LT_unicode_whitespace_p(uint32_t codepoint){
    return (codepoint >= UINT32_C(9) && codepoint <= UINT32_C(13))
        || codepoint == UINT32_C(0x85)
        || LT_unicode_category(codepoint)[0] == 'Z';
}

uint32_t LT_unicode_lowercase(uint32_t codepoint);
uint32_t LT_unicode_uppercase(uint32_t codepoint);
uint32_t LT_unicode_titlecase(uint32_t codepoint);
const uint32_t* LT_unicode_casefold(uint32_t codepoint, size_t* length_out);
const uint32_t* LT_unicode_full_lowercase(uint32_t codepoint, size_t* length_out);
const uint32_t* LT_unicode_full_titlecase(uint32_t codepoint, size_t* length_out);
const uint32_t* LT_unicode_full_uppercase(uint32_t codepoint, size_t* length_out);

#endif
