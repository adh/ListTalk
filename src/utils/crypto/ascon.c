/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/utils/crypto/ascon.h>

#include <string.h>

#define ASCON_HASH_RATE 8
#define ASCON_XOF_IV UINT64_C(0x0000080000cc0003)

static const uint8_t round_constants[12] = {
    0xf0, 0xe1, 0xd2, 0xc3, 0xb4, 0xa5,
    0x96, 0x87, 0x78, 0x69, 0x5a, 0x4b
};

static uint64_t rotr(uint64_t value, unsigned int shift){
    return (value >> shift) | (value << (64 - shift));
}

static uint64_t load_le(const uint8_t* bytes, size_t length){
    uint64_t value = 0;
    size_t i;

    for (i = 0; i < length; i++){
        value |= ((uint64_t)bytes[i]) << (8 * i);
    }
    return value;
}

static void store_le(uint8_t* bytes, uint64_t value, size_t length){
    size_t i;

    for (i = 0; i < length; i++){
        bytes[i] = (uint8_t)(value & UINT64_C(0xff));
        value >>= 8;
    }
}

static void ascon_round(uint64_t state[5], uint8_t constant){
    uint64_t x0 = state[0];
    uint64_t x1 = state[1];
    uint64_t x2 = state[2];
    uint64_t x3 = state[3];
    uint64_t x4 = state[4];
    uint64_t t0;
    uint64_t t1;
    uint64_t t2;
    uint64_t t3;
    uint64_t t4;

    x2 ^= (uint64_t)constant;

    x0 ^= x4;
    x4 ^= x3;
    x2 ^= x1;

    t0 = (~x0) & x1;
    t1 = (~x1) & x2;
    t2 = (~x2) & x3;
    t3 = (~x3) & x4;
    t4 = (~x4) & x0;

    x0 ^= t1;
    x1 ^= t2;
    x2 ^= t3;
    x3 ^= t4;
    x4 ^= t0;

    x1 ^= x0;
    x0 ^= x4;
    x3 ^= x2;
    x2 = ~x2;

    state[0] = x0 ^ rotr(x0, 19) ^ rotr(x0, 28);
    state[1] = x1 ^ rotr(x1, 61) ^ rotr(x1, 39);
    state[2] = x2 ^ rotr(x2, 1) ^ rotr(x2, 6);
    state[3] = x3 ^ rotr(x3, 10) ^ rotr(x3, 17);
    state[4] = x4 ^ rotr(x4, 7) ^ rotr(x4, 41);
}

void LT_Ascon_permute(uint64_t state[5], unsigned int rounds){
    unsigned int i;

    if (rounds > 12){
        rounds = 12;
    }
    for (i = 12 - rounds; i < 12; i++){
        ascon_round(state, round_constants[i]);
    }
}

void LT_AsconXOF128_init(LT_AsconXOF128* xof,
                         const uint8_t* input,
                         size_t length){
    xof->state[0] = ASCON_XOF_IV;
    xof->state[1] = 0;
    xof->state[2] = 0;
    xof->state[3] = 0;
    xof->state[4] = 0;
    LT_Ascon_permute(xof->state, 12);

    while (length >= ASCON_HASH_RATE){
        xof->state[0] ^= load_le(input, ASCON_HASH_RATE);
        LT_Ascon_permute(xof->state, 12);
        input += ASCON_HASH_RATE;
        length -= ASCON_HASH_RATE;
    }

    xof->state[0] ^= load_le(input, length);
    xof->state[0] ^= UINT64_C(1) << (8 * length);
    LT_Ascon_permute(xof->state, 12);

    memset(xof->buffer, 0, sizeof(xof->buffer));
    xof->buffer_offset = 0;
    xof->buffer_length = 0;
    xof->need_permute_before_block = 0;
}

void LT_AsconXOF128_squeeze(LT_AsconXOF128* xof,
                            uint8_t* output,
                            size_t length){
    while (length > 0){
        size_t available;
        size_t chunk;

        if (xof->buffer_offset == xof->buffer_length){
            if (xof->need_permute_before_block){
                LT_Ascon_permute(xof->state, 12);
            }
            store_le(xof->buffer, xof->state[0], sizeof(xof->buffer));
            xof->buffer_offset = 0;
            xof->buffer_length = sizeof(xof->buffer);
            xof->need_permute_before_block = 1;
        }

        available = xof->buffer_length - xof->buffer_offset;
        chunk = length < available ? length : available;
        memcpy(output, xof->buffer + xof->buffer_offset, chunk);
        output += chunk;
        length -= chunk;
        xof->buffer_offset += chunk;
    }
}
