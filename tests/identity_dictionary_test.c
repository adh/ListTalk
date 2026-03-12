/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/IdentityDictionary.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/SmallInteger.h>

#include <stdio.h>

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

static int test_new_dictionary_is_empty(void){
    LT_IdentityDictionary* dictionary = LT_IdentityDictionary_new();
    return expect(
        LT_IdentityDictionary_size(dictionary) == 0,
        "new dictionary has zero size"
    );
}

static int test_at_put_and_at_roundtrip(void){
    LT_IdentityDictionary* dictionary = LT_IdentityDictionary_new();
    LT_Value key = LT_SmallInteger_new(10);
    LT_Value value = LT_SmallInteger_new(42);
    LT_Value fetched = LT_NIL;

    LT_IdentityDictionary_atPut(dictionary, key, value);

    if (expect(LT_IdentityDictionary_size(dictionary) == 1, "insert increments size")){
        return 1;
    }
    if (expect(
        LT_IdentityDictionary_at(dictionary, key, &fetched),
        "at returns found for inserted key"
    )){
        return 1;
    }
    return expect(
        LT_Value_is_fixnum(fetched) && LT_SmallInteger_value(fetched) == 42,
        "fetched value matches inserted value"
    );
}

static int test_put_existing_key_overwrites_without_growth(void){
    LT_IdentityDictionary* dictionary = LT_IdentityDictionary_new();
    LT_Value key = LT_SmallInteger_new(1);
    LT_Value value = LT_SmallInteger_new(100);
    LT_Value replacement = LT_SmallInteger_new(200);
    LT_Value fetched = LT_NIL;

    LT_IdentityDictionary_atPut(dictionary, key, value);
    LT_IdentityDictionary_atPut(dictionary, key, replacement);

    if (expect(
        LT_IdentityDictionary_size(dictionary) == 1,
        "overwrite keeps size unchanged"
    )){
        return 1;
    }
    if (expect(LT_IdentityDictionary_at(dictionary, key, &fetched), "key still present")){
        return 1;
    }
    return expect(
        LT_Value_is_fixnum(fetched) && LT_SmallInteger_value(fetched) == 200,
        "overwrite updates value"
    );
}

static int test_identity_semantics_for_distinct_objects(void){
    LT_IdentityDictionary* dictionary = LT_IdentityDictionary_new();
    LT_Value key_a = LT_cons(LT_SmallInteger_new(1), LT_SmallInteger_new(2));
    LT_Value key_b = LT_cons(LT_SmallInteger_new(1), LT_SmallInteger_new(2));
    LT_Value value = LT_SmallInteger_new(7);

    LT_IdentityDictionary_atPut(dictionary, key_a, value);

    if (expect(
        !LT_IdentityDictionary_at(dictionary, key_b, NULL),
        "distinct but equal-shaped object is a miss"
    )){
        return 1;
    }
    return expect(
        LT_IdentityDictionary_at(dictionary, key_a, NULL),
        "original object identity is a hit"
    );
}

static int test_remove_existing_key(void){
    LT_IdentityDictionary* dictionary = LT_IdentityDictionary_new();
    LT_Value key = LT_SmallInteger_new(5);
    LT_Value value = LT_SmallInteger_new(9);
    LT_Value removed = LT_NIL;

    LT_IdentityDictionary_atPut(dictionary, key, value);

    if (expect(
        LT_IdentityDictionary_remove(dictionary, key, &removed),
        "remove returns true for existing key"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_fixnum(removed) && LT_SmallInteger_value(removed) == 9,
        "remove returns removed value"
    )){
        return 1;
    }
    if (expect(
        LT_IdentityDictionary_size(dictionary) == 0,
        "remove decrements dictionary size"
    )){
        return 1;
    }
    return expect(
        !LT_IdentityDictionary_at(dictionary, key, NULL),
        "removed key is no longer found"
    );
}

static int test_remove_missing_key(void){
    LT_IdentityDictionary* dictionary = LT_IdentityDictionary_new();
    LT_Value key = LT_SmallInteger_new(11);

    return expect(
        !LT_IdentityDictionary_remove(dictionary, key, NULL),
        "remove returns false for missing key"
    );
}

int main(void){
    int failures = 0;

    LT_init();

    failures += test_new_dictionary_is_empty();
    failures += test_at_put_and_at_roundtrip();
    failures += test_put_existing_key_overwrites_without_growth();
    failures += test_identity_semantics_for_distinct_objects();
    failures += test_remove_existing_key();
    failures += test_remove_missing_key();

    if (failures == 0){
        puts("identity dictionary tests passed");
        return 0;
    }

    fprintf(stderr, "%d identity dictionary test(s) failed\n", failures);
    return 1;
}
