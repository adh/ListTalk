/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__utils__crypto__sha256__
#define H__ListTalk__utils__crypto__sha256__

#include <ListTalk/macros/env_macros.h>

#include <stddef.h>
#include <stdint.h>

LT__BEGIN_DECLS

#define LT_SHA256_DIGEST_LENGTH 32

typedef struct LT_SHA256_s {
    uint32_t state[8];
    uint8_t buffer[64];
    size_t buffer_length;
    uint64_t total_length;
} LT_SHA256;

void LT_SHA256_init(LT_SHA256* sha256);
void LT_SHA256_update(LT_SHA256* sha256, const uint8_t* bytes, size_t length);
void LT_SHA256_finish(LT_SHA256* sha256,
                      uint8_t digest[LT_SHA256_DIGEST_LENGTH]);
void LT_SHA256_digest(const uint8_t* bytes,
                      size_t length,
                      uint8_t digest[LT_SHA256_DIGEST_LENGTH]);

LT__END_DECLS

#endif
