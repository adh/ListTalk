/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Boolean.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Reader.h>
#include <ListTalk/classes/Symbol.h>

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

static LT_Value read_one(const char* source){
    LT_Reader* reader = LT_Reader_new();
    LT_ReaderStream* stream = LT_ReaderStream_newForString(source);
    return LT_Reader_readObject(reader, stream);
}

static LT_Value eval_one(const char* source){
    LT_Environment* env = LT_new_base_environment();
    return LT_eval(read_one(source), env);
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

static int test_quote(void){
    LT_Value value = eval_one("(quote (+ 1 2))");

    if (expect(LT_Value_is_pair(value), "quote returns list")){
        return 1;
    }
    if (expect(LT_Value_is_symbol(LT_car(value)), "quote list first is symbol")){
        return 1;
    }

    return expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_object(LT_car(value))), "+") == 0,
        "quote does not evaluate"
    );
}

static int test_lambda_application(void){
    LT_Value value = eval_one("((lambda (x y) (+ x y)) 3 4)");
    return expect(
        LT_Value_is_fixnum(value) && LT_Value_fixnum_value(value) == 7,
        "closure application"
    );
}

static int test_if_special_form(void){
    LT_Value value = eval_one("(if () 1 2)");
    return expect(
        LT_Value_is_fixnum(value) && LT_Value_fixnum_value(value) == 2,
        "if false branch"
    );
}

static int test_define_special_form(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value result;

    (void)LT_eval(read_one("(define add1 (lambda (x) (+ x 1)))"), env);
    result = LT_eval(read_one("(add1 9)"), env);
    return expect(
        LT_Value_is_fixnum(result) && LT_Value_fixnum_value(result) == 10,
        "define binds value in current environment"
    );
}

static int test_set_bang_special_form(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value result;

    (void)LT_eval(read_one("(define x 10)"), env);
    (void)LT_eval(read_one("(set! x 42)"), env);
    result = LT_eval(read_one("x"), env);
    return expect(
        LT_Value_is_fixnum(result) && LT_Value_fixnum_value(result) == 42,
        "set! updates existing binding"
    );
}

static int test_set_bang_parent_binding(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value result;

    (void)LT_eval(read_one("(define x 1)"), env);
    (void)LT_eval(read_one("((lambda () (set! x 9)))"), env);
    result = LT_eval(read_one("x"), env);
    return expect(
        LT_Value_is_fixnum(result) && LT_Value_fixnum_value(result) == 9,
        "set! updates lexical parent binding"
    );
}

static int test_symbol_class_inherits_object(void){
    if (expect(LT_Symbol_class.superclasses != NULL, "symbol has superclass")){
        return 1;
    }
    if (expect(
        LT_Symbol_class.superclasses[0] == &LT_Object_class,
        "symbol superclass is Object"
    )){
        return 1;
    }
    return expect(
        LT_Symbol_class.superclasses[1] == NULL,
        "single direct superclass in native descriptors"
    );
}

static int test_boolean_constants(void){
    if (expect(LT_Value_is_boolean(LT_TRUE), "LT_TRUE is boolean")){
        return 1;
    }
    if (expect(LT_Value_is_boolean(LT_FALSE), "LT_FALSE is boolean")){
        return 1;
    }
    if (expect(LT_Value_boolean_value(LT_TRUE) == 1, "LT_TRUE value")){
        return 1;
    }
    if (expect(LT_Value_boolean_value(LT_FALSE) == 0, "LT_FALSE value")){
        return 1;
    }
    return expect(
        LT_Value_class(LT_TRUE) == &LT_Boolean_class
            && LT_Value_class(LT_FALSE) == &LT_Boolean_class,
        "boolean class mapping"
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
    failures += test_quote();
    failures += test_lambda_application();
    failures += test_if_special_form();
    failures += test_define_special_form();
    failures += test_set_bang_special_form();
    failures += test_set_bang_parent_binding();
    failures += test_symbol_class_inherits_object();
    failures += test_boolean_constants();

    if (failures == 0){
        puts("eval tests passed");
        return 0;
    }

    fprintf(stderr, "%d eval test(s) failed\n", failures);
    return 1;
}
