/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Reader.h>

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

static LT_Value read_one(const char* source){
    LT_Reader* reader = LT_Reader_new();
    LT_ReaderStream* stream = LT_ReaderStream_newForString(source);
    return LT_Reader_readObject(reader, stream);
}

static LT_Value eval_one(const char* source){
    return LT_eval(read_one(source), LT_get_shared_base_environment());
}

static int test_add(void){
    LT_Value value = eval_one("(+ 1 2 3)");
    return expect(
        LT_Value_is_fixnum(value) && LT_Value_fixnum_value(value) == 6,
        "addition"
    );
}

static int test_subtract_unary(void){
    LT_Value value = eval_one("(- 7)");
    return expect(
        LT_Value_is_fixnum(value) && LT_Value_fixnum_value(value) == -7,
        "unary subtraction"
    );
}

static int test_multiply(void){
    LT_Value value = eval_one("(* 4 5)");
    return expect(
        LT_Value_is_fixnum(value) && LT_Value_fixnum_value(value) == 20,
        "multiplication"
    );
}

static int test_divide(void){
    LT_Value value = eval_one("(/ 20 2 2)");
    return expect(
        LT_Value_is_fixnum(value) && LT_Value_fixnum_value(value) == 5,
        "division"
    );
}

static int test_symbol_lookup(void){
    LT_Value value = eval_one("+");
    return expect(
        LT_Value_is_primitive(value),
        "symbol lookup in base environment"
    );
}

int main(void){
    int failures = 0;

    LT_init();

    failures += test_add();
    failures += test_subtract_unary();
    failures += test_multiply();
    failures += test_divide();
    failures += test_symbol_lookup();

    if (failures == 0){
        puts("eval tests passed");
        return 0;
    }

    fprintf(stderr, "%d eval test(s) failed\n", failures);
    return 1;
}
