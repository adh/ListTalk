/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/utils/utf8.h>

static int utf8_continuation_valid(unsigned char ch){
    return ch != '\0' && LT_utf8_is_continuation(ch);
}

static int utf8_codepoint_valid(uint32_t codepoint){
    if (codepoint > UINT32_C(0x10ffff)){
        return 0;
    }
    return codepoint < UINT32_C(0xd800) || codepoint > UINT32_C(0xdfff);
}

size_t LT_utf8_sequence_length(unsigned char first){
    if (first < 0x80){
        return 1;
    }
    if (first >= 0xc2 && first <= 0xdf){
        return 2;
    }
    if (first >= 0xe0 && first <= 0xef){
        return 3;
    }
    if (first >= 0xf0 && first <= 0xf4){
        return 4;
    }
    return 0;
}

int LT_utf8_sequence_valid(const unsigned char* cursor, size_t length){
    unsigned char first = cursor[0];

    switch (length){
        case 1:
            return first < 0x80;
        case 2:
            return first >= 0xc2
                && first <= 0xdf
                && utf8_continuation_valid(cursor[1]);
        case 3:
            if (first < 0xe0 || first > 0xef){
                return 0;
            }
            if (!utf8_continuation_valid(cursor[1])
                || !utf8_continuation_valid(cursor[2])){
                return 0;
            }
            if (first == 0xe0){
                return cursor[1] >= 0xa0;
            }
            if (first == 0xed){
                return cursor[1] <= 0x9f;
            }
            return 1;
        case 4:
            if (first < 0xf0 || first > 0xf4){
                return 0;
            }
            if (!utf8_continuation_valid(cursor[1])
                || !utf8_continuation_valid(cursor[2])
                || !utf8_continuation_valid(cursor[3])){
                return 0;
            }
            if (first == 0xf0){
                return cursor[1] >= 0x90;
            }
            if (first == 0xf4){
                return cursor[1] <= 0x8f;
            }
            return 1;
        default:
            return 0;
    }
}

int LT_utf8_sequence_valid_bounded(const unsigned char* cursor,
                                   size_t available,
                                   size_t length){
    if (length > available){
        return 0;
    }
    return LT_utf8_sequence_valid(cursor, length);
}

uint32_t LT_utf8_decode_valid(const unsigned char* cursor, size_t length){
    switch (length){
        case 1:
            return cursor[0];
        case 2:
            return ((uint32_t)(cursor[0] & 0x1f) << 6)
                | (uint32_t)(cursor[1] & 0x3f);
        case 3:
            return ((uint32_t)(cursor[0] & 0x0f) << 12)
                | ((uint32_t)(cursor[1] & 0x3f) << 6)
                | (uint32_t)(cursor[2] & 0x3f);
        case 4:
            return ((uint32_t)(cursor[0] & 0x07) << 18)
                | ((uint32_t)(cursor[1] & 0x3f) << 12)
                | ((uint32_t)(cursor[2] & 0x3f) << 6)
                | (uint32_t)(cursor[3] & 0x3f);
        default:
            return LT_UTF8_REPLACEMENT_CODEPOINT;
    }
}

uint32_t LT_utf8_codepoint_at_bounded(const char* cursor, size_t available){
    const unsigned char* bytes = (const unsigned char*)cursor;
    size_t length;

    if (available == 0){
        return LT_UTF8_REPLACEMENT_CODEPOINT;
    }
    if (bytes[0] == '\0'){
        return 0;
    }

    length = LT_utf8_sequence_length(bytes[0]);
    if (length == 0 || !LT_utf8_sequence_valid_bounded(bytes, available, length)){
        return LT_UTF8_REPLACEMENT_CODEPOINT;
    }

    return LT_utf8_decode_valid(bytes, length);
}

uint32_t LT_utf8_codepoint_at(const char* cursor){
    const unsigned char* bytes = (const unsigned char*)cursor;
    size_t length;

    if (bytes[0] == '\0'){
        return 0;
    }

    length = LT_utf8_sequence_length(bytes[0]);
    if (length == 0 || !LT_utf8_sequence_valid(bytes, length)){
        return LT_UTF8_REPLACEMENT_CODEPOINT;
    }

    return LT_utf8_decode_valid(bytes, length);
}

const char* LT_utf8_next_bounded(const char* cursor, size_t available){
    const unsigned char* bytes = (const unsigned char*)cursor;
    size_t length;

    if (available == 0){
        return cursor;
    }
    if (bytes[0] == '\0'){
        return cursor + 1;
    }

    if (LT_utf8_is_continuation(bytes[0])){
        size_t offset = 1;

        while (offset < available && LT_utf8_is_continuation(bytes[offset])){
            offset++;
        }
        return cursor + offset;
    }

    length = LT_utf8_sequence_length(bytes[0]);
    if (length == 0 || !LT_utf8_sequence_valid_bounded(bytes, available, length)){
        return cursor + 1;
    }

    return cursor + length;
}

const char* LT_utf8_next(const char* cursor){
    const unsigned char* bytes = (const unsigned char*)cursor;
    size_t length;

    if (bytes[0] == '\0'){
        return cursor + 1;
    }

    if (LT_utf8_is_continuation(bytes[0])){
        do {
            cursor++;
            bytes++;
        } while (bytes[0] != '\0' && LT_utf8_is_continuation(bytes[0]));
        return cursor;
    }

    length = LT_utf8_sequence_length(bytes[0]);
    if (length == 0 || !LT_utf8_sequence_valid(bytes, length)){
        return cursor + 1;
    }

    return cursor + length;
}

size_t LT_utf8_encode(uint32_t codepoint, char buffer[4]){
    if (!utf8_codepoint_valid(codepoint)){
        return 0;
    }
    if (codepoint <= UINT32_C(0x7f)){
        buffer[0] = (char)codepoint;
        return 1;
    }
    if (codepoint <= UINT32_C(0x7ff)){
        buffer[0] = (char)(0xc0 | (codepoint >> 6));
        buffer[1] = (char)(0x80 | (codepoint & 0x3f));
        return 2;
    }
    if (codepoint <= UINT32_C(0xffff)){
        buffer[0] = (char)(0xe0 | (codepoint >> 12));
        buffer[1] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        buffer[2] = (char)(0x80 | (codepoint & 0x3f));
        return 3;
    }
    buffer[0] = (char)(0xf0 | (codepoint >> 18));
    buffer[1] = (char)(0x80 | ((codepoint >> 12) & 0x3f));
    buffer[2] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
    buffer[3] = (char)(0x80 | (codepoint & 0x3f));
    return 4;
}
