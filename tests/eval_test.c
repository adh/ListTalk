/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Boolean.h>
#include <ListTalk/classes/Character.h>
#include <ListTalk/classes/Float.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Reader.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/classes/Vector.h>
#include <ListTalk/macros/arg_macros.h>

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

static LT_Value read_one(const char* source){
    LT_Reader* reader = LT_Reader_new();
    LT_ReaderStream* stream = LT_ReaderStream_newForString(source);
    return LT_Reader_readObject(reader, stream);
}

static LT_Value eval_one(const char* source){
    LT_Environment* env = LT_new_base_environment();
    return LT_eval(read_one(source), env, NULL);
}

static int test_add(void){
    LT_Value value = eval_one("(+ 1 2 3)");
    return expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 6,
        "addition"
    );
}

static int test_subtract_unary(void){
    LT_Value value = eval_one("(- 7)");
    return expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == -7,
        "unary subtraction"
    );
}

static int test_multiply(void){
    LT_Value value = eval_one("(* 4 5)");
    return expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 20,
        "multiplication"
    );
}

static int test_divide(void){
    LT_Value value = eval_one("(/ 20 2 2)");
    return expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 5,
        "division"
    );
}

static int test_add_float_mixed(void){
    LT_Value value = eval_one("(+ 1.5 2)");
    return expect(
        LT_Float_p(value) && LT_Float_value(value) == 3.5,
        "mixed addition returns float"
    );
}

static int test_divide_float_mixed(void){
    LT_Value value = eval_one("(/ 5.0 2)");
    return expect(
        LT_Float_p(value) && LT_Float_value(value) == 2.5,
        "mixed division returns float"
    );
}

static int test_subtract_unary_float(void){
    LT_Value value = eval_one("(- 1.5)");
    return expect(
        LT_Float_p(value) && LT_Float_value(value) == -1.5,
        "unary subtraction on float"
    );
}

static int test_integer_divide_still_fixnum(void){
    LT_Value value = eval_one("(/ 5 2)");
    return expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 2,
        "integer division remains fixnum"
    );
}

static int test_symbol_lookup(void){
    LT_Value value = eval_one("+");
    return expect(
        LT_Primitive_p(value),
        "symbol lookup in base environment"
    );
}

static int test_display_primitive_returns_argument(void){
    LT_Value value = eval_one("(display \"hello\")");

    if (expect(LT_String_p(value), "display returns value")){
        return 1;
    }
    return expect(
        strcmp(LT_String_value_cstr(LT_String_from_value(value)), "hello") == 0,
        "display returns exact string argument"
    );
}

static int test_keyword_self_evaluating_when_unbound(void){
    LT_Value value = eval_one(":token");

    if (expect(LT_Symbol_p(value), "keyword eval result is symbol")){
        return 1;
    }
    if (expect(
        LT_Symbol_package(LT_Symbol_from_value(value)) == LT_PACKAGE_KEYWORD,
        "keyword symbol package"
    )){
        return 1;
    }
    return expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_value(value)), "token") == 0,
        "keyword symbol name"
    );
}

static int test_type_of_primitive(void){
    LT_Value value = eval_one("(type-of 1)");
    return expect(
        (LT_Class*)LT_VALUE_POINTER_VALUE(value) == &LT_SmallInteger_class,
        "type-of returns class object"
    );
}

static int test_cons_primitive(void){
    LT_Value value = eval_one("(cons 1 2)");

    if (expect(LT_Pair_p(value), "cons returns pair")){
        return 1;
    }
    if (expect(
        LT_Value_is_fixnum(LT_car(value)) && LT_SmallInteger_value(LT_car(value)) == 1,
        "cons sets car"
    )){
        return 1;
    }
    return expect(
        LT_Value_is_fixnum(LT_cdr(value)) && LT_SmallInteger_value(LT_cdr(value)) == 2,
        "cons sets cdr"
    );
}

static int test_car_primitive(void){
    LT_Value value = eval_one("(car '(7 8 9))");
    return expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 7,
        "car returns first element"
    );
}

static int test_cdr_primitive(void){
    LT_Value value = eval_one("(cdr '(7 8 9))");

    if (expect(LT_Pair_p(value), "cdr of non-empty list returns pair")){
        return 1;
    }
    if (expect(
        LT_Value_is_fixnum(LT_car(value)) && LT_SmallInteger_value(LT_car(value)) == 8,
        "cdr first element"
    )){
        return 1;
    }
    return expect(
        LT_Value_is_fixnum(LT_car(LT_cdr(value)))
            && LT_SmallInteger_value(LT_car(LT_cdr(value))) == 9,
        "cdr second element"
    );
}

static int test_pair_predicate_primitive(void){
    LT_Value pair_value = eval_one("(pair? '(1 . 2))");
    LT_Value fixnum_value = eval_one("(pair? 1)");

    if (expect(
        LT_Value_is_boolean(pair_value) && LT_Value_boolean_value(pair_value),
        "pair? true for pair"
    )){
        return 1;
    }
    return expect(
        LT_Value_is_boolean(fixnum_value) && !LT_Value_boolean_value(fixnum_value),
        "pair? false for non-pair"
    );
}

static int test_string_predicate_primitive(void){
    LT_Value string_value = eval_one("(string? \"abc\")");
    LT_Value fixnum_value = eval_one("(string? 1)");

    if (expect(
        LT_Value_is_boolean(string_value) && LT_Value_boolean_value(string_value),
        "string? true for strings"
    )){
        return 1;
    }
    return expect(
        LT_Value_is_boolean(fixnum_value) && !LT_Value_boolean_value(fixnum_value),
        "string? false for non-strings"
    );
}

static int test_string_length_primitive(void){
    LT_Value value = eval_one("(string-length \"hello\")");
    return expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 5,
        "string-length"
    );
}

static int test_string_ref_primitive(void){
    LT_Value value = eval_one("(string-ref \"ABC\" 1)");
    if (expect(LT_Character_p(value), "string-ref returns character")){
        return 1;
    }
    return expect(LT_Character_value(value) == 'B', "string-ref character value");
}

static int test_character_predicate_primitive(void){
    LT_Value character_value = eval_one("(character? (string-ref \"x\" 0))");
    LT_Value fixnum_value = eval_one("(character? 1)");

    if (expect(
        LT_Value_is_boolean(character_value)
            && LT_Value_boolean_value(character_value),
        "character? true for characters"
    )){
        return 1;
    }
    return expect(
        LT_Value_is_boolean(fixnum_value) && !LT_Value_boolean_value(fixnum_value),
        "character? false for non-characters"
    );
}

static int test_string_to_character_list_primitive(void){
    LT_Value value = eval_one("(string->list \"ABC\")");

    if (expect(LT_Pair_p(value), "string->list returns list")){
        return 1;
    }
    if (expect(
        LT_Character_p(LT_car(value))
            && LT_Character_value(LT_car(value)) == 'A',
        "first character is A"
    )){
        return 1;
    }
    value = LT_cdr(value);
    if (expect(
        LT_Pair_p(value)
            && LT_Character_p(LT_car(value))
            && LT_Character_value(LT_car(value)) == 'B',
        "second character is B"
    )){
        return 1;
    }
    value = LT_cdr(value);
    if (expect(
        LT_Pair_p(value)
            && LT_Character_p(LT_car(value))
            && LT_Character_value(LT_car(value)) == 'C',
        "third character is C"
    )){
        return 1;
    }
    return expect(LT_cdr(value) == LT_NIL, "list ends with nil");
}

static int test_character_list_to_string_primitive(void){
    LT_Value value = eval_one(
        "(list->string (string->list \"hello\"))"
    );

    if (expect(
        LT_Value_class(value) == &LT_String_class,
        "list->string returns string"
    )){
        return 1;
    }
    return expect(
        strcmp(LT_String_value_cstr(LT_String_from_value(value)), "hello") == 0,
        "list->string round-trip"
    );
}

static int test_string_append_primitive(void){
    LT_Value value = eval_one("(string-append \"ab\" \"cd\")");

    if (expect(LT_Value_class(value) == &LT_String_class, "string-append returns string")){
        return 1;
    }
    return expect(
        strcmp(LT_String_value_cstr(LT_String_from_value(value)), "abcd") == 0,
        "string-append concatenates"
    );
}

static int test_vector_predicate_primitive(void){
    LT_Value vector_value = eval_one("(vector? #(1 2))");
    LT_Value fixnum_value = eval_one("(vector? 1)");

    if (expect(
        LT_Value_is_boolean(vector_value) && LT_Value_boolean_value(vector_value),
        "vector? true for vectors"
    )){
        return 1;
    }
    return expect(
        LT_Value_is_boolean(fixnum_value) && !LT_Value_boolean_value(fixnum_value),
        "vector? false for non-vectors"
    );
}

static int test_vector_length_primitive(void){
    LT_Value value = eval_one("(vector-length #(1 2 3))");
    return expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 3,
        "vector-length"
    );
}

static int test_vector_constructor_and_ref_primitive(void){
    LT_Value value = eval_one("(vector-ref (vector 4 5 6) 2)");
    return expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 6,
        "vector constructor and vector-ref"
    );
}

static int test_make_vector_primitive(void){
    LT_Value value = eval_one("(vector-ref (make-vector 3 9) 1)");
    return expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 9,
        "make-vector with fill value"
    );
}

static int test_vector_set_bang_primitive(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value result;

    (void)LT_eval(read_one("(define v (vector 1 2 3))"), env, NULL);
    (void)LT_eval(read_one("(vector-set! v 1 99)"), env, NULL);
    result = LT_eval(read_one("(vector-ref v 1)"), env, NULL);
    return expect(
        LT_Value_is_fixnum(result) && LT_SmallInteger_value(result) == 99,
        "vector-set! mutates vector"
    );
}

static int test_quote(void){
    LT_Value value = eval_one("(quote (+ 1 2))");

    if (expect(LT_Pair_p(value), "quote returns list")){
        return 1;
    }
    if (expect(LT_Symbol_p(LT_car(value)), "quote list first is symbol")){
        return 1;
    }

    return expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_value(LT_car(value))), "+") == 0,
        "quote does not evaluate"
    );
}

static int test_quote_reader_syntax(void){
    LT_Value value = eval_one("'(+ 1 2)");

    if (expect(LT_Pair_p(value), "quote reader syntax returns list")){
        return 1;
    }
    if (expect(LT_Symbol_p(LT_car(value)), "quote reader list first symbol")){
        return 1;
    }

    return expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_value(LT_car(value))), "+") == 0,
        "quote reader syntax does not evaluate"
    );
}

static int test_lambda_application(void){
    LT_Value value = eval_one("((lambda (x y) (+ x y)) 3 4)");
    return expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 7,
        "closure application"
    );
}

static int test_lambda_rest_parameter_dotted(void){
    LT_Value value = eval_one("((lambda (x . rest) rest) 1 2 3)");

    if (expect(LT_Pair_p(value), "dotted rest returns list")){
        return 1;
    }
    if (expect(
        LT_Value_is_fixnum(LT_car(value)) && LT_SmallInteger_value(LT_car(value)) == 2,
        "dotted rest first element"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_fixnum(LT_car(LT_cdr(value)))
            && LT_SmallInteger_value(LT_car(LT_cdr(value))) == 3,
        "dotted rest second element"
    )){
        return 1;
    }
    return expect(
        LT_cdr(LT_cdr(value)) == LT_NIL,
        "dotted rest tail terminates"
    );
}

static int test_lambda_rest_parameter_symbol(void){
    LT_Value value = eval_one("((lambda args args) 4 5)");

    if (expect(LT_Pair_p(value), "symbol rest returns list")){
        return 1;
    }
    if (expect(
        LT_Value_is_fixnum(LT_car(value)) && LT_SmallInteger_value(LT_car(value)) == 4,
        "symbol rest first element"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_fixnum(LT_car(LT_cdr(value)))
            && LT_SmallInteger_value(LT_car(LT_cdr(value))) == 5,
        "symbol rest second element"
    )){
        return 1;
    }
    return expect(
        LT_cdr(LT_cdr(value)) == LT_NIL,
        "symbol rest tail terminates"
    );
}

static int test_tail_call_optimization_deep_recursion(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value input = LT_NIL;
    LT_Value result;
    int i;

    for (i = 0; i < 20000; i += 1){
        input = LT_cons(LT_NIL, input);
    }

    LT_Environment_bind(env, LT_Symbol_new("input"), input, 0);
    (void)LT_eval(
        read_one("(define len-loop (lambda (xs acc) (if xs (len-loop (cdr xs) (+ acc 1)) acc)))"),
        env,
        NULL
    );
    result = LT_eval(read_one("(len-loop input 0)"), env, NULL);
    return expect(
        LT_Value_is_fixnum(result) && LT_SmallInteger_value(result) == 20000,
        "tail recursion stays bounded and returns expected value"
    );
}

static int test_if_special_form(void){
    LT_Value value = eval_one("(if () 1 2)");
    return expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 2,
        "if false branch"
    );
}

static int test_define_special_form(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value result;

    (void)LT_eval(read_one("(define add1 (lambda (x) (+ x 1)))"), env, NULL);
    result = LT_eval(read_one("(add1 9)"), env, NULL);
    return expect(
        LT_Value_is_fixnum(result) && LT_SmallInteger_value(result) == 10,
        "define binds value in current environment"
    );
}

static int test_set_bang_special_form(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value result;

    (void)LT_eval(read_one("(define x 10)"), env, NULL);
    (void)LT_eval(read_one("(set! x 42)"), env, NULL);
    result = LT_eval(read_one("x"), env, NULL);
    return expect(
        LT_Value_is_fixnum(result) && LT_SmallInteger_value(result) == 42,
        "set! updates existing binding"
    );
}

static int test_set_bang_parent_binding(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value result;

    (void)LT_eval(read_one("(define x 1)"), env, NULL);
    (void)LT_eval(read_one("((lambda () (set! x 9)))"), env, NULL);
    result = LT_eval(read_one("x"), env, NULL);
    return expect(
        LT_Value_is_fixnum(result) && LT_SmallInteger_value(result) == 9,
        "set! updates lexical parent binding"
    );
}

static int test_macro_special_form_constructs_macro(void){
    LT_Value value = eval_one("(macro (lambda (x) x))");
    return expect(
        LT_Macro_p(value),
        "macro special form creates macro value"
    );
}

static int test_macro_expansion_is_evaluated(void){
    LT_Value value = eval_one("((macro (lambda (x) x)) (+ 1 2))");
    return expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 3,
        "macro expansion is evaluated"
    );
}

static int test_macro_expansion_uses_call_environment(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value result;

    (void)LT_eval(read_one("(define x 42)"), env, NULL);
    (void)LT_eval(read_one("(define id-macro (macro (lambda (x) x)))"), env, NULL);
    result = LT_eval(read_one("(id-macro x)"), env, NULL);
    return expect(
        LT_Value_is_fixnum(result) && LT_SmallInteger_value(result) == 42,
        "macro expansion evaluates in caller environment"
    );
}

static int test_compiler_fold_non_constant_symbol_returns_invalid(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value symbol_x = LT_Symbol_new("x");
    LT_Value folded;

    LT_Environment_bind(env, symbol_x, LT_SmallInteger_new(7), 0);
    folded = LT_compiler_fold_expression(symbol_x, env);
    return expect(
        folded == LT_INVALID,
        "compiler fold returns invalid for non-constant symbol"
    );
}

static int test_compiler_fold_application_folds_operator_and_arguments(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value folded;
    LT_Value args;

    LT_Environment_bind(
        env,
        LT_Symbol_new("x"),
        LT_SmallInteger_new(5),
        LT_ENV_BINDING_FLAG_CONSTANT
    );
    folded = LT_compiler_fold_expression(read_one("(+ x y)"), env);

    if (expect(LT_Pair_p(folded), "compiler fold returns folded application")){
        return 1;
    }
    if (expect(LT_Primitive_p(LT_car(folded)), "folded operator becomes value")){
        return 1;
    }

    args = LT_cdr(folded);
    if (expect(
        LT_Value_is_fixnum(LT_car(args)) && LT_SmallInteger_value(LT_car(args)) == 5,
        "constant argument is folded to bound value"
    )){
        return 1;
    }
    return expect(
        LT_car(LT_cdr(args)) == LT_INVALID,
        "undefined argument is folded to invalid marker"
    );
}

static int test_compiler_fold_special_form_uses_special_form_reference(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value folded;
    LT_Value arguments;

    LT_Environment_bind(
        env,
        LT_Symbol_new("x"),
        LT_TRUE,
        LT_ENV_BINDING_FLAG_CONSTANT
    );
    folded = LT_compiler_fold_expression(read_one("(if x (+ 1 2) y)"), env);

    if (expect(LT_Pair_p(folded), "special form fold returns list form")){
        return 1;
    }
    if (expect(
        LT_SpecialForm_p(LT_car(folded)),
        "special form operator is replaced by special form value"
    )){
        return 1;
    }

    arguments = LT_cdr(folded);
    if (expect(
        LT_car(arguments) == LT_TRUE,
        "special form expression folds constant condition"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_fixnum(LT_car(LT_cdr(arguments)))
            && LT_SmallInteger_value(LT_car(LT_cdr(arguments))) == 3,
        "special form expression constant-folds pure nested application"
    )){
        return 1;
    }
    return expect(
        LT_car(LT_cdr(LT_cdr(arguments))) == LT_INVALID,
        "special form expression folds unresolved symbol to invalid"
    );
}

static int test_compiler_fold_expands_macros(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value folded;
    LT_Value macro_value;

    macro_value = LT_eval(read_one("(macro (lambda (x) x))"), env, NULL);
    LT_Environment_bind(
        env,
        LT_Symbol_new("id-macro"),
        macro_value,
        LT_ENV_BINDING_FLAG_CONSTANT
    );

    folded = LT_compiler_fold_expression(read_one("(id-macro (+ 1 2))"), env);

    return expect(
        LT_Value_is_fixnum(folded) && LT_SmallInteger_value(folded) == 3,
        "macro expansion result is folded through pure primitive"
    );
}

static int test_compiler_fold_pure_primitive_constant_folds(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value folded = LT_compiler_fold_expression(read_one("(+ 1 2 3)"), env);

    return expect(
        LT_Value_is_fixnum(folded) && LT_SmallInteger_value(folded) == 6,
        "compiler fold constant-folds pure primitive application"
    );
}

static int test_compiler_fold_impure_primitive_is_not_constant_folded(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value folded = LT_compiler_fold_expression(read_one("(error \"boom\")"), env);

    if (expect(
        LT_Pair_p(folded),
        "compiler fold keeps impure primitive as application"
    )){
        return 1;
    }
    return expect(
        LT_Primitive_p(LT_car(folded)),
        "compiler fold resolves impure operator but does not execute it"
    );
}

static int test_macroexpand_special_form(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value macro_value;
    LT_Value expanded;
    LT_Value arguments;

    macro_value = LT_eval(read_one("(macro (lambda (x) x))"), env, NULL);
    LT_Environment_bind(
        env,
        LT_Symbol_new("id-macro"),
        macro_value,
        LT_ENV_BINDING_FLAG_CONSTANT
    );

    expanded = LT_eval(
        read_one("(macroexpand '(id-macro (+ 1 2)) (get-current-environment))"),
        env,
        NULL
    );
    if (expect(LT_Pair_p(expanded), "macroexpand primitive returns form")){
        return 1;
    }
    if (expect(
        LT_Symbol_p(LT_car(expanded))
            && strcmp(LT_Symbol_name(LT_Symbol_from_value(LT_car(expanded))), "+") == 0,
        "macroexpand primitive expands to application syntax"
    )){
        return 1;
    }

    arguments = LT_cdr(expanded);
    if (expect(
        LT_Value_is_fixnum(LT_car(arguments))
            && LT_SmallInteger_value(LT_car(arguments)) == 1,
        "macroexpand primitive keeps first argument"
    )){
        return 1;
    }
    return expect(
        LT_Value_is_fixnum(LT_car(LT_cdr(arguments)))
            && LT_SmallInteger_value(LT_car(LT_cdr(arguments))) == 2,
        "macroexpand primitive keeps second argument"
    );
}

static int test_fold_expression_special_form(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value folded;
    LT_Value arguments;

    LT_Environment_bind(
        env,
        LT_Symbol_new("x"),
        LT_SmallInteger_new(5),
        LT_ENV_BINDING_FLAG_CONSTANT
    );

    folded = LT_eval(
        read_one("(fold-expression '(+ x y) (get-current-environment))"),
        env,
        NULL
    );
    if (expect(LT_Pair_p(folded), "fold-expression primitive returns form")){
        return 1;
    }
    if (expect(
        LT_Primitive_p(LT_car(folded)),
        "fold-expression primitive resolves operator"
    )){
        return 1;
    }

    arguments = LT_cdr(folded);
    if (expect(
        LT_Value_is_fixnum(LT_car(arguments))
            && LT_SmallInteger_value(LT_car(arguments)) == 5,
        "fold-expression primitive resolves constant symbol"
    )){
        return 1;
    }
    return expect(
        LT_car(LT_cdr(arguments)) == LT_INVALID,
        "fold-expression primitive marks unresolved symbol invalid"
    );
}

static int test_get_current_environment_special_form(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value folded;
    LT_Value arguments;

    LT_Environment_bind(
        env,
        LT_Symbol_new("x"),
        LT_SmallInteger_new(7),
        LT_ENV_BINDING_FLAG_CONSTANT
    );
    (void)LT_eval(
        read_one("(define captured-env (get-current-environment))"),
        env,
        NULL
    );

    folded = LT_eval(
        read_one("(fold-expression '(+ x y) captured-env)"),
        env,
        NULL
    );
    if (expect(LT_Pair_p(folded), "captured environment can be reused")){
        return 1;
    }
    arguments = LT_cdr(folded);
    return expect(
        LT_Value_is_fixnum(LT_car(arguments))
            && LT_SmallInteger_value(LT_car(arguments)) == 7,
        "get-current-environment captures lexical environment"
    );
}

static int test_catch_returns_body_value_without_throw(void){
    LT_Value value = eval_one("(catch :t (+ 1 2))");
    return expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 3,
        "catch returns body value when no throw happens"
    );
}

static int test_throw_is_caught_by_matching_tag(void){
    LT_Value value = eval_one("(catch :t (throw :t 42) 99)");
    return expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 42,
        "throw transfers control to matching catch"
    );
}

static int test_throw_skips_to_outer_matching_catch(void){
    LT_Value value = eval_one("(catch :outer (catch :inner (throw :outer 7)))");
    return expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 7,
        "throw skips non-matching inner catch"
    );
}

static int test_unwind_protect_runs_cleanup_on_normal_path(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value protected_result;
    LT_Value side_effect;

    (void)LT_eval(read_one("(define x 0)"), env, NULL);
    protected_result = LT_eval(
        read_one("(unwind-protect 7 (set! x 1))"),
        env,
        NULL
    );
    side_effect = LT_eval(read_one("x"), env, NULL);

    if (expect(
        LT_Value_is_fixnum(protected_result)
            && LT_SmallInteger_value(protected_result) == 7,
        "unwind-protect returns protected value on normal path"
    )){
        return 1;
    }
    return expect(
        LT_Value_is_fixnum(side_effect) && LT_SmallInteger_value(side_effect) == 1,
        "unwind-protect runs cleanup on normal path"
    );
}

static int test_unwind_protect_runs_cleanup_on_throw_path(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value caught_result;
    LT_Value side_effect;

    (void)LT_eval(read_one("(define x 0)"), env, NULL);
    caught_result = LT_eval(
        read_one("(catch :t (unwind-protect (throw :t 9) (set! x 1)))"),
        env,
        NULL
    );
    side_effect = LT_eval(read_one("x"), env, NULL);

    if (expect(
        LT_Value_is_fixnum(caught_result)
            && LT_SmallInteger_value(caught_result) == 9,
        "unwind-protect rethrows and outer catch receives value"
    )){
        return 1;
    }
    return expect(
        LT_Value_is_fixnum(side_effect) && LT_SmallInteger_value(side_effect) == 1,
        "unwind-protect runs cleanup during throw"
    );
}

static int test_handler_bind_special_form_binds_handler_for_body(void){
    LT_Value value = eval_one(
        "(catch :t "
        "  (handler-bind (lambda (c) (throw :t c)) "
        "    (error \"boom\")))"
    );

    if (expect(LT_String_p(value), "handler-bind catches error condition")){
        return 1;
    }
    return expect(
        strcmp(LT_String_value_cstr(LT_String_from_value(value)), "boom") == 0,
        "error forwards string condition through handler-bind"
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

static int test_character_api_uses_unicode_codepoints(void){
    LT_Value value = LT_Character_new(UINT32_C(0x1f600));

    if (expect(LT_Character_p(value), "LT_Character_new returns character")){
        return 1;
    }
    return expect(
        LT_Character_value(value) == UINT32_C(0x1f600),
        "LT_Character stores full 21-bit code point"
    );
}

int main(void){
    int failures = 0;

    LT_init();

    failures += test_add();
    failures += test_subtract_unary();
    failures += test_multiply();
    failures += test_divide();
    failures += test_add_float_mixed();
    failures += test_divide_float_mixed();
    failures += test_subtract_unary_float();
    failures += test_integer_divide_still_fixnum();
    failures += test_symbol_lookup();
    failures += test_display_primitive_returns_argument();
    failures += test_keyword_self_evaluating_when_unbound();
    failures += test_type_of_primitive();
    failures += test_cons_primitive();
    failures += test_car_primitive();
    failures += test_cdr_primitive();
    failures += test_pair_predicate_primitive();
    failures += test_string_predicate_primitive();
    failures += test_string_length_primitive();
    failures += test_string_ref_primitive();
    failures += test_character_predicate_primitive();
    failures += test_string_to_character_list_primitive();
    failures += test_character_list_to_string_primitive();
    failures += test_string_append_primitive();
    failures += test_vector_predicate_primitive();
    failures += test_vector_length_primitive();
    failures += test_vector_constructor_and_ref_primitive();
    failures += test_make_vector_primitive();
    failures += test_vector_set_bang_primitive();
    failures += test_quote();
    failures += test_quote_reader_syntax();
    failures += test_lambda_application();
    failures += test_lambda_rest_parameter_dotted();
    failures += test_lambda_rest_parameter_symbol();
    failures += test_tail_call_optimization_deep_recursion();
    failures += test_if_special_form();
    failures += test_define_special_form();
    failures += test_set_bang_special_form();
    failures += test_set_bang_parent_binding();
    failures += test_macro_special_form_constructs_macro();
    failures += test_macro_expansion_is_evaluated();
    failures += test_macro_expansion_uses_call_environment();
    failures += test_compiler_fold_non_constant_symbol_returns_invalid();
    failures += test_compiler_fold_application_folds_operator_and_arguments();
    failures += test_compiler_fold_special_form_uses_special_form_reference();
    failures += test_compiler_fold_expands_macros();
    failures += test_compiler_fold_pure_primitive_constant_folds();
    failures += test_compiler_fold_impure_primitive_is_not_constant_folded();
    failures += test_macroexpand_special_form();
    failures += test_fold_expression_special_form();
    failures += test_get_current_environment_special_form();
    failures += test_catch_returns_body_value_without_throw();
    failures += test_throw_is_caught_by_matching_tag();
    failures += test_throw_skips_to_outer_matching_catch();
    failures += test_unwind_protect_runs_cleanup_on_normal_path();
    failures += test_unwind_protect_runs_cleanup_on_throw_path();
    failures += test_handler_bind_special_form_binds_handler_for_body();
    failures += test_symbol_class_inherits_object();
    failures += test_boolean_constants();
    failures += test_character_api_uses_unicode_codepoints();

    if (failures == 0){
        puts("eval tests passed");
        return 0;
    }

    fprintf(stderr, "%d eval test(s) failed\n", failures);
    return 1;
}
