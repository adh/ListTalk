/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__utils__base64__
#define H__ListTalk__utils__base64__

#include <ListTalk/macros/env_macros.h>

#include <stddef.h>
#include <stdint.h>

LT__BEGIN_DECLS

char* LT_base64_encode(const uint8_t* bytes,
                       size_t length,
                       int uri_safe,
                       int include_padding,
                       size_t* length_out);
uint8_t* LT_base64_decode(const char* string,
                          size_t length,
                          int uri_safe,
                          int require_padding,
                          size_t* length_out);

LT__END_DECLS

#endif
