/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Ales Hakl
 */

#ifndef H__ListTalk__utils__unicode_data__
#define H__ListTalk__utils__unicode_data__

#include <stdint.h>

const char* LT_unicode_category(uint32_t codepoint);
uint32_t LT_unicode_lowercase(uint32_t codepoint);
uint32_t LT_unicode_uppercase(uint32_t codepoint);
uint32_t LT_unicode_titlecase(uint32_t codepoint);

#endif
