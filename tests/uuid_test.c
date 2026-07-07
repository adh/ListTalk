/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Dictionary.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/UUID.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int fail(const char* message){
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

static int expect(int condition, const char* message){
    if (!condition){
        return fail(message);
    }
    return 0;
}

static int test_parse_and_format(void){
    LT_String* source = LT_String_new_cstr("550e8400-e29b-41d4-a716-446655440000");
    LT_UUID* uuid = LT_UUID_from_string(source);
    LT_String* formatted = LT_UUID_as_string(uuid);

    if (expect(
        strcmp(LT_String_value_cstr(formatted), LT_String_value_cstr(source)) == 0,
        "UUID formats canonically"
    )){
        return 1;
    }
    if (expect(LT_UUID_at(uuid, 0) == 0x55, "first UUID byte")){
        return 1;
    }
    return expect(LT_UUID_at(uuid, 15) == 0x00, "last UUID byte");
}

static int test_uppercase_parse_formats_lowercase(void){
    LT_UUID* uuid = LT_UUID_from_string(
        LT_String_new_cstr("550E8400-E29B-41D4-A716-446655440000")
    );
    LT_String* formatted = LT_UUID_as_string(uuid);

    return expect(
        strcmp(
            LT_String_value_cstr(formatted),
            "550e8400-e29b-41d4-a716-446655440000"
        ) == 0,
        "uppercase UUID parses and formats lowercase"
    );
}

static int test_bytes_are_copied(void){
    uint8_t bytes[LT_UUID_BYTE_LENGTH] = {
        0x55, 0x0e, 0x84, 0x00, 0xe2, 0x9b, 0x41, 0xd4,
        0xa7, 0x16, 0x44, 0x66, 0x55, 0x44, 0x00, 0x00
    };
    LT_UUID* uuid = LT_UUID_new(bytes);

    bytes[0] = 0xff;
    return expect(LT_UUID_at(uuid, 0) == 0x55, "UUID constructor copies bytes");
}

static int test_equal_hash_and_dictionary_key(void){
    LT_UUID* uuid_a = LT_UUID_from_string(
        LT_String_new_cstr("550e8400-e29b-41d4-a716-446655440000")
    );
    LT_UUID* uuid_b = LT_UUID_from_string(
        LT_String_new_cstr("550e8400-e29b-41d4-a716-446655440000")
    );
    LT_Dictionary* dictionary = LT_Dictionary_new();
    LT_Value fetched = LT_NIL;

    if (expect(
        LT_Value_equal_p((LT_Value)(uintptr_t)uuid_a, (LT_Value)(uintptr_t)uuid_b),
        "equal UUIDs compare equal"
    )){
        return 1;
    }
    if (expect(
        LT_Value_hash((LT_Value)(uintptr_t)uuid_a)
            == LT_Value_hash((LT_Value)(uintptr_t)uuid_b),
        "equal UUIDs have equal hash"
    )){
        return 1;
    }

    LT_Dictionary_atPut(
        dictionary,
        (LT_Value)(uintptr_t)uuid_a,
        LT_SmallInteger_new(23)
    );
    if (expect(
        LT_Dictionary_at(dictionary, (LT_Value)(uintptr_t)uuid_b, &fetched),
        "equal UUID dictionary lookup"
    )){
        return 1;
    }
    return expect(
        LT_Value_is_fixnum(fetched) && LT_SmallInteger_value(fetched) == 23,
        "equal UUID dictionary lookup returns value"
    );
}

static int test_compare(void){
    LT_UUID* lower = LT_UUID_from_string(
        LT_String_new_cstr("00000000-0000-0000-0000-000000000001")
    );
    LT_UUID* higher = LT_UUID_from_string(
        LT_String_new_cstr("00000000-0000-0000-0000-000000000002")
    );

    if (expect(LT_UUID_compare(lower, higher) < 0, "UUID less than")){
        return 1;
    }
    if (expect(LT_UUID_compare(higher, lower) > 0, "UUID greater than")){
        return 1;
    }
    return expect(LT_UUID_compare(lower, lower) == 0, "UUID equal compare");
}

static int test_random_v4_shape(void){
    LT_UUID* uuid = LT_UUID_random_v4();
    const uint8_t* bytes = LT_UUID_bytes(uuid);

    if (expect((bytes[6] & 0xf0) == 0x40, "random UUID has version 4")){
        return 1;
    }
    return expect((bytes[8] & 0xc0) == 0x80, "random UUID has RFC variant");
}

int main(void){
    int failures = 0;

    LT_INIT();

    failures += test_parse_and_format();
    failures += test_uppercase_parse_formats_lowercase();
    failures += test_bytes_are_copied();
    failures += test_equal_hash_and_dictionary_key();
    failures += test_compare();
    failures += test_random_v4_shape();

    if (failures == 0){
        puts("uuid tests passed");
        return 0;
    }

    fprintf(stderr, "%d UUID test(s) failed\n", failures);
    return 1;
}
