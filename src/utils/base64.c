/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/utils/base64.h>
#include <ListTalk/vm/error.h>

#include <gc.h>
#include <stdint.h>
#include <string.h>

static const char base64_alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char base64_uri_alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static int base64_decode_char(unsigned char ch, int uri_safe){
    if (ch >= 'A' && ch <= 'Z'){
        return (int)(ch - 'A');
    }
    if (ch >= 'a' && ch <= 'z'){
        return (int)(ch - 'a') + 26;
    }
    if (ch >= '0' && ch <= '9'){
        return (int)(ch - '0') + 52;
    }
    if (ch == '+'){
        if (uri_safe){
            return -1;
        }
        return 62;
    }
    if (ch == '/'){
        if (uri_safe){
            return -1;
        }
        return 63;
    }
    if (ch == '-'){
        return uri_safe ? 62 : -1;
    }
    if (ch == '_'){
        return uri_safe ? 63 : -1;
    }
    return -1;
}

static int ascii_whitespace_p(unsigned char ch){
    return ch == ' '
        || ch == '\t'
        || ch == '\n'
        || ch == '\r'
        || ch == '\f'
        || ch == '\v';
}

char* LT_base64_encode(const uint8_t* bytes,
                       size_t length,
                       int uri_safe,
                       int include_padding,
                       size_t* length_out){
    const char* alphabet = uri_safe ? base64_uri_alphabet : base64_alphabet;
    size_t encoded_length;
    size_t padded_length;
    char* encoded;
    size_t input_offset = 0;
    size_t output_offset = 0;

    if (length > ((SIZE_MAX - 1) / 4) * 3){
        LT_error("Base64 input too large");
    }

    padded_length = ((length + 2) / 3) * 4;
    encoded_length = padded_length;
    if (!include_padding && length % 3 != 0){
        encoded_length -= 3 - (length % 3);
    }
    encoded = GC_MALLOC_ATOMIC(encoded_length + 1);

    while (input_offset < length){
        size_t remaining = length - input_offset;
        uint32_t value = (uint32_t)bytes[input_offset] << 16;

        if (remaining > 1){
            value |= (uint32_t)bytes[input_offset + 1] << 8;
        }
        if (remaining > 2){
            value |= (uint32_t)bytes[input_offset + 2];
        }

        encoded[output_offset++] = alphabet[(value >> 18) & 0x3f];
        encoded[output_offset++] = alphabet[(value >> 12) & 0x3f];
        if (remaining > 1){
            encoded[output_offset++] = alphabet[(value >> 6) & 0x3f];
        } else if (include_padding){
            encoded[output_offset++] = '=';
        }
        if (remaining > 2){
            encoded[output_offset++] = alphabet[value & 0x3f];
        } else if (include_padding){
            encoded[output_offset++] = '=';
        }

        input_offset += 3;
    }

    encoded[encoded_length] = '\0';
    *length_out = encoded_length;
    return encoded;
}

uint8_t* LT_base64_decode(const char* string,
                          size_t length,
                          int uri_safe,
                          int require_padding,
                          size_t* length_out){
    char* compacted;
    size_t compacted_length = 0;
    size_t scan_offset;
    size_t body_length;
    size_t padding = 0;
    size_t remainder;
    size_t decoded_length;
    uint8_t* decoded;
    size_t input_offset;
    size_t output_offset = 0;

    compacted = GC_MALLOC_ATOMIC(length == 0 ? 1 : length);
    for (scan_offset = 0; scan_offset < length; scan_offset++){
        unsigned char ch = (unsigned char)string[scan_offset];

        if (!ascii_whitespace_p(ch)){
            compacted[compacted_length++] = (char)ch;
        }
    }
    string = compacted;
    length = compacted_length;

    if (length >= 1 && string[length - 1] == '='){
        padding++;
    }
    if (length >= 2 && string[length - 2] == '='){
        padding++;
    }
    if (padding > 0 && length % 4 != 0){
        LT_error("Invalid base64");
    }
    if (require_padding && length % 4 != 0){
        LT_error("Invalid base64");
    }
    if (padding == 0 && length % 4 == 1){
        LT_error("Invalid base64");
    }

    body_length = length - padding;
    remainder = body_length % 4;
    decoded_length = (body_length / 4) * 3;
    if (remainder > 0){
        decoded_length += remainder - 1;
    }
    decoded = GC_MALLOC_ATOMIC(decoded_length == 0 ? 1 : decoded_length);

    for (input_offset = 0; input_offset < body_length; input_offset += 4){
        int sextets[4];
        uint32_t value;
        size_t input_count = body_length - input_offset;
        size_t i;

        if (input_count > 4){
            input_count = 4;
        }

        for (i = 0; i < 4; i++){
            if (i >= input_count){
                sextets[i] = 0;
                continue;
            }
            unsigned char ch = (unsigned char)string[input_offset + i];

            sextets[i] = base64_decode_char(ch, uri_safe);
            if (sextets[i] < 0){
                LT_error("Invalid base64");
            }
        }

        value = ((uint32_t)sextets[0] << 18)
            | ((uint32_t)sextets[1] << 12)
            | ((uint32_t)sextets[2] << 6)
            | (uint32_t)sextets[3];

        if (input_offset + input_count == body_length){
            size_t omitted_padding = input_count == 4 ? padding : 4 - input_count;

            if ((omitted_padding == 1 && (sextets[2] & 0x03) != 0)
                || (omitted_padding == 2 && (sextets[1] & 0x0f) != 0)){
                LT_error("Invalid base64");
            }
        }

        if (output_offset < decoded_length){
            decoded[output_offset++] = (uint8_t)((value >> 16) & 0xff);
        }
        if (output_offset < decoded_length){
            decoded[output_offset++] = (uint8_t)((value >> 8) & 0xff);
        }
        if (output_offset < decoded_length){
            decoded[output_offset++] = (uint8_t)(value & 0xff);
        }
    }

    *length_out = decoded_length;
    return decoded;
}
