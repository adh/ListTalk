/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/utils/hex.h>
#include <ListTalk/vm/error.h>

#include <gc.h>
#include <stdint.h>

static const char hex_alphabet[] = "0123456789abcdef";

static int ascii_whitespace_p(unsigned char ch){
    return ch == ' '
        || ch == '\t'
        || ch == '\n'
        || ch == '\r'
        || ch == '\f'
        || ch == '\v';
}

static int hex_decode_char(unsigned char ch){
    if (ch >= '0' && ch <= '9'){
        return (int)(ch - '0');
    }
    if (ch >= 'a' && ch <= 'f'){
        return (int)(ch - 'a') + 10;
    }
    if (ch >= 'A' && ch <= 'F'){
        return (int)(ch - 'A') + 10;
    }
    return -1;
}

char* LT_hex_encode(const uint8_t* bytes, size_t length, size_t* length_out){
    size_t encoded_length;
    char* encoded;
    size_t i;

    if (length > (SIZE_MAX - 1) / 2){
        LT_error("Hex input too large");
    }

    encoded_length = length * 2;
    encoded = GC_MALLOC_ATOMIC(encoded_length + 1);
    for (i = 0; i < length; i++){
        encoded[i * 2] = hex_alphabet[bytes[i] >> 4];
        encoded[i * 2 + 1] = hex_alphabet[bytes[i] & 0x0f];
    }
    encoded[encoded_length] = '\0';
    *length_out = encoded_length;
    return encoded;
}

uint8_t* LT_hex_decode(const char* string, size_t length, size_t* length_out){
    char* compacted;
    size_t compacted_length = 0;
    size_t scan_offset;
    size_t decoded_length;
    uint8_t* decoded;
    size_t i;

    compacted = GC_MALLOC_ATOMIC(length == 0 ? 1 : length);
    for (scan_offset = 0; scan_offset < length; scan_offset++){
        unsigned char ch = (unsigned char)string[scan_offset];

        if (!ascii_whitespace_p(ch)){
            compacted[compacted_length++] = (char)ch;
        }
    }

    if (compacted_length % 2 != 0){
        LT_error("Invalid hex");
    }

    decoded_length = compacted_length / 2;
    decoded = GC_MALLOC_ATOMIC(decoded_length == 0 ? 1 : decoded_length);
    for (i = 0; i < decoded_length; i++){
        int high = hex_decode_char((unsigned char)compacted[i * 2]);
        int low = hex_decode_char((unsigned char)compacted[i * 2 + 1]);

        if (high < 0 || low < 0){
            LT_error("Invalid hex");
        }
        decoded[i] = (uint8_t)((high << 4) | low);
    }

    *length_out = decoded_length;
    return decoded;
}
