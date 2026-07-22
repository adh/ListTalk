/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/utils/crypto/sha256.h>

#include <string.h>

static const uint32_t k[64] = {
    UINT32_C(0x428a2f98), UINT32_C(0x71374491), UINT32_C(0xb5c0fbcf), UINT32_C(0xe9b5dba5),
    UINT32_C(0x3956c25b), UINT32_C(0x59f111f1), UINT32_C(0x923f82a4), UINT32_C(0xab1c5ed5),
    UINT32_C(0xd807aa98), UINT32_C(0x12835b01), UINT32_C(0x243185be), UINT32_C(0x550c7dc3),
    UINT32_C(0x72be5d74), UINT32_C(0x80deb1fe), UINT32_C(0x9bdc06a7), UINT32_C(0xc19bf174),
    UINT32_C(0xe49b69c1), UINT32_C(0xefbe4786), UINT32_C(0x0fc19dc6), UINT32_C(0x240ca1cc),
    UINT32_C(0x2de92c6f), UINT32_C(0x4a7484aa), UINT32_C(0x5cb0a9dc), UINT32_C(0x76f988da),
    UINT32_C(0x983e5152), UINT32_C(0xa831c66d), UINT32_C(0xb00327c8), UINT32_C(0xbf597fc7),
    UINT32_C(0xc6e00bf3), UINT32_C(0xd5a79147), UINT32_C(0x06ca6351), UINT32_C(0x14292967),
    UINT32_C(0x27b70a85), UINT32_C(0x2e1b2138), UINT32_C(0x4d2c6dfc), UINT32_C(0x53380d13),
    UINT32_C(0x650a7354), UINT32_C(0x766a0abb), UINT32_C(0x81c2c92e), UINT32_C(0x92722c85),
    UINT32_C(0xa2bfe8a1), UINT32_C(0xa81a664b), UINT32_C(0xc24b8b70), UINT32_C(0xc76c51a3),
    UINT32_C(0xd192e819), UINT32_C(0xd6990624), UINT32_C(0xf40e3585), UINT32_C(0x106aa070),
    UINT32_C(0x19a4c116), UINT32_C(0x1e376c08), UINT32_C(0x2748774c), UINT32_C(0x34b0bcb5),
    UINT32_C(0x391c0cb3), UINT32_C(0x4ed8aa4a), UINT32_C(0x5b9cca4f), UINT32_C(0x682e6ff3),
    UINT32_C(0x748f82ee), UINT32_C(0x78a5636f), UINT32_C(0x84c87814), UINT32_C(0x8cc70208),
    UINT32_C(0x90befffa), UINT32_C(0xa4506ceb), UINT32_C(0xbef9a3f7), UINT32_C(0xc67178f2)
};

static uint32_t rotr(uint32_t value, unsigned int shift){
    return (value >> shift) | (value << (32 - shift));
}

static uint32_t load_be32(const uint8_t* bytes){
    return ((uint32_t)bytes[0] << 24)
        | ((uint32_t)bytes[1] << 16)
        | ((uint32_t)bytes[2] << 8)
        | (uint32_t)bytes[3];
}

static void store_be32(uint8_t* bytes, uint32_t value){
    bytes[0] = (uint8_t)(value >> 24);
    bytes[1] = (uint8_t)(value >> 16);
    bytes[2] = (uint8_t)(value >> 8);
    bytes[3] = (uint8_t)value;
}

static void store_be64(uint8_t* bytes, uint64_t value){
    size_t i;

    for (i = 0; i < 8; i++){
        bytes[7 - i] = (uint8_t)value;
        value >>= 8;
    }
}

static void sha256_compress(LT_SHA256* sha256, const uint8_t block[64]){
    uint32_t w[64];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
    uint32_t h;
    size_t i;

    for (i = 0; i < 16; i++){
        w[i] = load_be32(block + (i * 4));
    }
    for (i = 16; i < 64; i++){
        uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    a = sha256->state[0];
    b = sha256->state[1];
    c = sha256->state[2];
    d = sha256->state[3];
    e = sha256->state[4];
    f = sha256->state[5];
    g = sha256->state[6];
    h = sha256->state[7];

    for (i = 0; i < 64; i++){
        uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + ch + k[i] + w[i];
        uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    sha256->state[0] += a;
    sha256->state[1] += b;
    sha256->state[2] += c;
    sha256->state[3] += d;
    sha256->state[4] += e;
    sha256->state[5] += f;
    sha256->state[6] += g;
    sha256->state[7] += h;
}

void LT_SHA256_init(LT_SHA256* sha256){
    sha256->state[0] = UINT32_C(0x6a09e667);
    sha256->state[1] = UINT32_C(0xbb67ae85);
    sha256->state[2] = UINT32_C(0x3c6ef372);
    sha256->state[3] = UINT32_C(0xa54ff53a);
    sha256->state[4] = UINT32_C(0x510e527f);
    sha256->state[5] = UINT32_C(0x9b05688c);
    sha256->state[6] = UINT32_C(0x1f83d9ab);
    sha256->state[7] = UINT32_C(0x5be0cd19);
    sha256->buffer_length = 0;
    sha256->total_length = 0;
}

void LT_SHA256_update(LT_SHA256* sha256, const uint8_t* bytes, size_t length){
    sha256->total_length += (uint64_t)length;

    if (sha256->buffer_length > 0){
        size_t available = sizeof(sha256->buffer) - sha256->buffer_length;
        size_t chunk = length < available ? length : available;

        memcpy(sha256->buffer + sha256->buffer_length, bytes, chunk);
        sha256->buffer_length += chunk;
        bytes += chunk;
        length -= chunk;

        if (sha256->buffer_length == sizeof(sha256->buffer)){
            sha256_compress(sha256, sha256->buffer);
            sha256->buffer_length = 0;
        }
    }

    while (length >= sizeof(sha256->buffer)){
        sha256_compress(sha256, bytes);
        bytes += sizeof(sha256->buffer);
        length -= sizeof(sha256->buffer);
    }

    if (length > 0){
        memcpy(sha256->buffer, bytes, length);
        sha256->buffer_length = length;
    }
}

void LT_SHA256_finish(LT_SHA256* sha256,
                      uint8_t digest[LT_SHA256_DIGEST_LENGTH]){
    uint64_t bit_length = sha256->total_length * 8;
    size_t i;

    sha256->buffer[sha256->buffer_length++] = 0x80;
    if (sha256->buffer_length > 56){
        memset(sha256->buffer + sha256->buffer_length, 0, 64 - sha256->buffer_length);
        sha256_compress(sha256, sha256->buffer);
        sha256->buffer_length = 0;
    }

    memset(sha256->buffer + sha256->buffer_length, 0, 56 - sha256->buffer_length);
    store_be64(sha256->buffer + 56, bit_length);
    sha256_compress(sha256, sha256->buffer);
    sha256->buffer_length = 0;

    for (i = 0; i < 8; i++){
        store_be32(digest + (i * 4), sha256->state[i]);
    }
}

void LT_SHA256_digest(const uint8_t* bytes,
                      size_t length,
                      uint8_t digest[LT_SHA256_DIGEST_LENGTH]){
    LT_SHA256 sha256;

    LT_SHA256_init(&sha256);
    LT_SHA256_update(&sha256, bytes, length);
    LT_SHA256_finish(&sha256, digest);
}
