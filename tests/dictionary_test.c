/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Dictionary.h>
#include <ListTalk/classes/Float.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Vector.h>

#include <stdio.h>
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

static int test_string_keys_use_equal_semantics(void){
    LT_Dictionary* dictionary = LT_Dictionary_new();
    LT_Value key_a = (LT_Value)(uintptr_t)LT_String_new_cstr("alpha");
    LT_Value key_b = (LT_Value)(uintptr_t)LT_String_new_cstr("alpha");
    LT_Value value = LT_SmallInteger_new(42);
    LT_Value fetched = LT_NIL;

    LT_Dictionary_atPut(dictionary, key_a, value);

    if (expect(LT_Dictionary_size(dictionary) == 1, "dictionary size after string insert")){
        return 1;
    }
    if (expect(LT_Dictionary_at(dictionary, key_b, &fetched), "equal string key lookup")){
        return 1;
    }
    return expect(
        LT_Value_is_fixnum(fetched) && LT_SmallInteger_value(fetched) == 42,
        "equal string key returns stored value"
    );
}

static int test_pair_keys_use_structural_equal_semantics(void){
    LT_Dictionary* dictionary = LT_Dictionary_new();
    LT_Value key_a = LT_cons(LT_SmallInteger_new(1), LT_SmallInteger_new(2));
    LT_Value key_b = LT_cons(LT_SmallInteger_new(1), LT_SmallInteger_new(2));
    LT_Value value = LT_SmallInteger_new(7);
    LT_Value fetched = LT_NIL;

    LT_Dictionary_atPut(dictionary, key_a, value);

    if (expect(LT_Dictionary_at(dictionary, key_b, &fetched), "equal pair key lookup")){
        return 1;
    }
    return expect(
        LT_Value_is_fixnum(fetched) && LT_SmallInteger_value(fetched) == 7,
        "equal pair key returns stored value"
    );
}

static int test_numeric_cross_type_keys_match(void){
    LT_Dictionary* dictionary = LT_Dictionary_new();
    LT_Value key_fixnum = LT_SmallInteger_new(1);
    LT_Value key_float = LT_Float_new(1.0);
    LT_Value value = LT_SmallInteger_new(99);
    LT_Value fetched = LT_NIL;

    LT_Dictionary_atPut(dictionary, key_fixnum, value);

    if (expect(LT_Dictionary_at(dictionary, key_float, &fetched), "1.0 matches key 1")){
        return 1;
    }
    return expect(
        LT_Value_is_fixnum(fetched) && LT_SmallInteger_value(fetched) == 99,
        "numeric cross-type lookup returns value"
    );
}

static int test_vector_keys_use_structural_equal_semantics(void){
    LT_Dictionary* dictionary = LT_Dictionary_new();
    LT_Vector* vector_a = LT_Vector_new(2);
    LT_Vector* vector_b = LT_Vector_new(2);
    LT_Value value = LT_SmallInteger_new(5);

    LT_Vector_atPut(vector_a, 0, LT_SmallInteger_new(1));
    LT_Vector_atPut(vector_a, 1, LT_SmallInteger_new(2));
    LT_Vector_atPut(vector_b, 0, LT_SmallInteger_new(1));
    LT_Vector_atPut(vector_b, 1, LT_SmallInteger_new(2));

    LT_Dictionary_atPut(
        dictionary,
        (LT_Value)(uintptr_t)vector_a,
        value
    );

    return expect(
        LT_Dictionary_at(dictionary, (LT_Value)(uintptr_t)vector_b, NULL),
        "equal vectors match dictionary key"
    );
}

static int test_remove_uses_equal_semantics(void){
    LT_Dictionary* dictionary = LT_Dictionary_new();
    LT_Value key_a = LT_cons(LT_SmallInteger_new(1), LT_SmallInteger_new(3));
    LT_Value key_b = LT_cons(LT_SmallInteger_new(1), LT_SmallInteger_new(3));
    LT_Value value = LT_SmallInteger_new(55);
    LT_Value removed = LT_NIL;

    LT_Dictionary_atPut(dictionary, key_a, value);

    if (expect(
        LT_Dictionary_remove(dictionary, key_b, &removed),
        "remove finds equal key"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_fixnum(removed) && LT_SmallInteger_value(removed) == 55,
        "remove returns removed value"
    )){
        return 1;
    }
    if (expect(LT_Dictionary_size(dictionary) == 0, "remove decrements size")){
        return 1;
    }
    return expect(
        !LT_Dictionary_at(dictionary, key_a, NULL),
        "removed key not found"
    );
}

static int test_equal_p_still_handles_strings_lists_vectors(void){
    LT_Value string_a = (LT_Value)(uintptr_t)LT_String_new_cstr("x");
    LT_Value string_b = (LT_Value)(uintptr_t)LT_String_new_cstr("x");
    LT_Value list_a = LT_cons(LT_SmallInteger_new(1), LT_cons(LT_SmallInteger_new(2), LT_NIL));
    LT_Value list_b = LT_cons(LT_SmallInteger_new(1), LT_cons(LT_SmallInteger_new(2), LT_NIL));
    LT_Vector* vector_a = LT_Vector_new(2);
    LT_Vector* vector_b = LT_Vector_new(2);

    LT_Vector_atPut(vector_a, 0, LT_SmallInteger_new(1));
    LT_Vector_atPut(vector_a, 1, LT_SmallInteger_new(2));
    LT_Vector_atPut(vector_b, 0, LT_SmallInteger_new(1));
    LT_Vector_atPut(vector_b, 1, LT_SmallInteger_new(2));

    if (expect(LT_Value_equal_p(string_a, string_b), "string equal")){
        return 1;
    }
    if (expect(LT_Value_equal_p(list_a, list_b), "pair/list equal")){
        return 1;
    }
    return expect(
        LT_Value_equal_p((LT_Value)(uintptr_t)vector_a, (LT_Value)(uintptr_t)vector_b),
        "vector equal"
    );
}

int main(void){
    int failures = 0;

    LT_init();

    failures += test_string_keys_use_equal_semantics();
    failures += test_pair_keys_use_structural_equal_semantics();
    failures += test_numeric_cross_type_keys_match();
    failures += test_vector_keys_use_structural_equal_semantics();
    failures += test_remove_uses_equal_semantics();
    failures += test_equal_p_still_handles_strings_lists_vectors();

    if (failures == 0){
        puts("dictionary tests passed");
        return 0;
    }

    fprintf(stderr, "%d dictionary test(s) failed\n", failures);
    return 1;
}
