/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__utils__crypto__ascon__
#define H__ListTalk__utils__crypto__ascon__

#include <ListTalk/macros/env_macros.h>

#include <stddef.h>
#include <stdint.h>

LT__BEGIN_DECLS

#define LT_ASCON_XOF128_SEED_LENGTH 32

typedef struct LT_AsconXOF128_s {
    uint64_t state[5];
    uint8_t buffer[8];
    size_t buffer_offset;
    size_t buffer_length;
    int need_permute_before_block;
} LT_AsconXOF128;

void LT_Ascon_permute(uint64_t state[5], unsigned int rounds);
void LT_AsconXOF128_init(LT_AsconXOF128* xof,
                         const uint8_t* input,
                         size_t length);
void LT_AsconXOF128_squeeze(LT_AsconXOF128* xof,
                            uint8_t* output,
                            size_t length);

LT__END_DECLS

#endif
