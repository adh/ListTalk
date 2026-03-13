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

LT_DEFINE_PRIMITIVE(
    primitive_test_pair_sum_method,
    "test-pair-sum-method",
    "(self)",
    "Test helper method: sum pair car and cdr."
){
    LT_Value cursor = arguments;
    LT_Value self;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    return LT_Number_add2(LT_car(self), LT_cdr(self));
}

LT_DEFINE_PRIMITIVE(
    primitive_test_object_class_name_method,
    "test-object-class-name-method",
    "(self)",
    "Test helper method: return receiver class name."
){
    LT_Value cursor = arguments;
    LT_Value self;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Value_class(self)->name;
}

LT_DEFINE_PRIMITIVE(
    primitive_test_pair_override_method,
    "test-pair-override-method",
    "(self)",
    "Test helper method: return override marker."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)self;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Symbol_new("PairOverride");
}

static int list_contains_symbol_name(LT_Value list, const char* name){
    LT_Value cursor = list;

    while (cursor != LT_NIL){
        LT_Value element;

        if (!LT_Pair_p(cursor)){
            return 0;
        }
        element = LT_car(cursor);
        if (LT_Symbol_p(element)
            && strcmp(LT_Symbol_name(LT_Symbol_from_value(element)), name) == 0){
            return 1;
        }
        cursor = LT_cdr(cursor);
    }

    return 0;
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

static int test_native_class_lookup(void){
    LT_Value value = eval_one("SmallInteger");
    return expect(
        (LT_Class*)LT_VALUE_POINTER_VALUE(value) == &LT_SmallInteger_class,
        "native class is bound as constant in base environment"
    );
}

static int test_class_slots_primitive(void){
    LT_Value slots = eval_one("(class-slots Pair)");

    if (expect(LT_Pair_p(slots), "class-slots returns non-empty list for Pair")){
        return 1;
    }
    if (expect(
        list_contains_symbol_name(slots, "car"),
        "class-slots includes car"
    )){
        return 1;
    }
    return expect(
        list_contains_symbol_name(slots, "cdr"),
        "class-slots includes cdr"
    );
}

static int test_send_primitive_uses_direct_method_dictionary(void){
    LT_Value selector = LT_Symbol_new_in(LT_PACKAGE_KEYWORD, "sum");
    LT_Value result;

    LT_Class_addMethod(
        &LT_Pair_class,
        selector,
        LT_Primitive_from_static(&primitive_test_pair_sum_method)
    );

    result = eval_one("[ '(1 . 2) sum]");
    return expect(
        LT_Value_is_fixnum(result) && LT_SmallInteger_value(result) == 3,
        "send dispatches to direct method"
    );
}

static int test_send_primitive_uses_precedence_lookup_and_cache(void){
    LT_Value selector = LT_Symbol_new_in(LT_PACKAGE_KEYWORD, "class-name");
    LT_IdentityDictionary* pair_cache;
    LT_Value cached = LT_NIL;
    LT_Value result;

    LT_Class_addMethod(
        &LT_Object_class,
        selector,
        LT_Primitive_from_static(&primitive_test_object_class_name_method)
    );

    result = eval_one("[ '(1 . 2) class-name]");
    if (expect(
        LT_Symbol_p(result)
            && strcmp(LT_Symbol_name(LT_Symbol_from_value(result)), "Pair") == 0,
        "send resolves method on superclass precedence list"
    )){
        return 1;
    }

    pair_cache = LT_IdentityDictionary_from_value(LT_Pair_class.method_cache);
    if (expect(
        LT_IdentityDictionary_at(pair_cache, selector, &cached),
        "resolved method is cached in receiver class method_cache"
    )){
        return 1;
    }
    return expect(
        cached == LT_Primitive_from_static(&primitive_test_object_class_name_method),
        "method_cache stores resolved method"
    );
}

static int test_basic_object_and_class_methods(void){
    LT_Value object_class = eval_one("[1 class]");
    LT_Value class_slots = eval_one("[Class slots]");
    LT_Value lookup_hit = eval_one("[Pair lookupSelector: :car]");
    LT_Value lookup_miss = eval_one("[Pair lookupSelector: :does-not-exist]");

    if (expect(
        (LT_Class*)LT_VALUE_POINTER_VALUE(object_class) == &LT_SmallInteger_class,
        "Object>>class returns receiver class"
    )){
        return 1;
    }
    if (expect(
        LT_Primitive_p(lookup_hit),
        "Class>>lookupSelector: returns method when selector exists"
    )){
        return 1;
    }
    if (expect(
        lookup_miss == LT_NIL,
        "Class>>lookupSelector: returns nil when selector missing"
    )){
        return 1;
    }
    return expect(
        list_contains_symbol_name(class_slots, "name"),
        "Class>>slots returns slot list"
    );
}

static int test_basic_pair_methods(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value result;

    (void)LT_eval(read_one("(define p (cons 1 2))"), env, NULL);
    result = LT_eval(read_one("[p car]"), env, NULL);
    if (expect(
        LT_Value_is_fixnum(result) && LT_SmallInteger_value(result) == 1,
        "Pair>>car"
    )){
        return 1;
    }
    (void)LT_eval(read_one("[p car: 42]"), env, NULL);
    result = LT_eval(read_one("[p car]"), env, NULL);
    return expect(
        LT_Value_is_fixnum(result) && LT_SmallInteger_value(result) == 42,
        "Pair>>car: mutates pair"
    );
}

static int test_basic_string_and_vector_methods(void){
    LT_Value string_length = eval_one("[\"abc\" length]");
    LT_Value string_char = eval_one("[\"abc\" at: 1]");
    LT_Environment* env = LT_new_base_environment();
    LT_Value vector_value;

    (void)LT_eval(read_one("(define v (vector 4 5 6))"), env, NULL);
    (void)LT_eval(read_one("[v at: 1 put: 99]"), env, NULL);
    vector_value = LT_eval(read_one("[v at: 1]"), env, NULL);

    if (expect(
        LT_Value_is_fixnum(string_length) && LT_SmallInteger_value(string_length) == 3,
        "String>>length"
    )){
        return 1;
    }
    if (expect(
        LT_Character_p(string_char) && LT_Character_value(string_char) == 'b',
        "String>>at:"
    )){
        return 1;
    }
    return expect(
        LT_Value_is_fixnum(vector_value) && LT_SmallInteger_value(vector_value) == 99,
        "Vector>>at:put: and Vector>>at:"
    );
}

static int test_make_class_primitive(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value new_class;
    LT_Value class_slots;
    LT_Value subclass_slots;
    LT_Value class_predicate;

    new_class = LT_eval(
        read_one("(make-class 'Point (list Object) '(x y))"),
        env,
        NULL
    );
    LT_Environment_bind(env, LT_Symbol_new("Point"), new_class, 0);

    class_predicate = LT_eval(read_one("(class? Point)"), env, NULL);
    class_slots = LT_eval(read_one("[Point slots]"), env, NULL);
    subclass_slots = LT_eval(
        read_one("[(make-class 'PairPlus (list Pair) '(z)) slots]"),
        env,
        NULL
    );

    if (expect(
        LT_Value_is_boolean(class_predicate) && LT_Value_boolean_value(class_predicate),
        "make-class returns class object"
    )){
        return 1;
    }
    if (expect(
        list_contains_symbol_name(class_slots, "x")
            && list_contains_symbol_name(class_slots, "y"),
        "make-class installs declared dynamic slots"
    )){
        return 1;
    }
    return expect(
        list_contains_symbol_name(subclass_slots, "car")
            && list_contains_symbol_name(subclass_slots, "cdr")
            && list_contains_symbol_name(subclass_slots, "z"),
        "make-class includes inherited slots from primary superclass"
    );
}

static int test_make_instance_primitive(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value instance;

    (void)LT_eval(
        read_one("(define Point (make-class 'Point (list Object) '(x y)))"),
        env,
        NULL
    );
    instance = LT_eval(read_one("(make-instance Point)"), env, NULL);

    return expect(
        LT_Value_class(instance) == LT_Class_from_object(
            LT_eval(read_one("Point"), env, NULL)
        ),
        "make-instance allocates empty instance for allocatable class"
    );
}

static int test_class_alloc_method(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value instance;

    (void)LT_eval(
        read_one("(define Point (make-class 'Point (list Object) '(x y)))"),
        env,
        NULL
    );
    instance = LT_eval(read_one("[Point alloc]"), env, NULL);

    return expect(
        LT_Value_class(instance) == LT_Class_from_object(
            LT_eval(read_one("Point"), env, NULL)
        ),
        "Class>>alloc allocates empty instance for allocatable class"
    );
}

static int test_make_instance_non_allocatable_class_errors(void){
    LT_Value value = eval_one(
        "(catch :t "
        "  (handler-bind (lambda (c) (throw :t c)) "
        "    (make-instance Pair)))"
    );

    if (expect(LT_String_p(value), "make-instance on Pair signals condition")){
        return 1;
    }
    return expect(
        strcmp(
            LT_String_value_cstr(LT_String_from_value(value)),
            "Class is not allocatable"
        ) == 0,
        "make-instance on non-allocatable class raises error"
    );
}

static int test_class_alloc_non_allocatable_class_errors(void){
    LT_Value value = eval_one(
        "(catch :t "
        "  (handler-bind (lambda (c) (throw :t c)) "
        "    [Pair alloc]))"
    );

    if (expect(LT_String_p(value), "Class>>alloc on Pair signals condition")){
        return 1;
    }
    return expect(
        strcmp(
            LT_String_value_cstr(LT_String_from_value(value)),
            "Class is not allocatable"
        ) == 0,
        "Class>>alloc on non-allocatable class raises error"
    );
}

static int test_class_add_method_with_selector_method(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value result;

    (void)LT_eval(
        read_one("(define m [Pair lookupSelector: :car])"),
        env,
        NULL
    );
    (void)LT_eval(
        read_one("[Pair addMethod: m withSelector: :first]"),
        env,
        NULL
    );
    result = LT_eval(read_one("['(9 . 8) first]"), env, NULL);

    return expect(
        LT_Value_is_fixnum(result) && LT_SmallInteger_value(result) == 9,
        "Class>>addMethod:withSelector: installs callable method"
    );
}

static int test_class_add_method_invalidates_method_cache(void){
    LT_Value selector = LT_Symbol_new_in(LT_PACKAGE_KEYWORD, "class-name");
    LT_Value result;

    LT_Class_addMethod(
        &LT_Object_class,
        selector,
        LT_Primitive_from_static(&primitive_test_object_class_name_method)
    );
    (void)eval_one("[ '(1 . 2) class-name]");

    LT_Class_addMethod(
        &LT_Pair_class,
        selector,
        LT_Primitive_from_static(&primitive_test_pair_override_method)
    );
    result = eval_one("[ '(1 . 2) class-name]");

    return expect(
        LT_Symbol_p(result)
            && strcmp(LT_Symbol_name(LT_Symbol_from_value(result)), "PairOverride") == 0,
        "LT_Class_addMethod invalidates stale method cache entries"
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

static int test_numeric_equal_primitive(void){
    LT_Value true_value = eval_one("(= 1 1 1.0)");
    LT_Value false_value = eval_one("(= 1 2)");

    if (expect(
        LT_Value_is_boolean(true_value) && LT_Value_boolean_value(true_value),
        "= true when all numbers are equal"
    )){
        return 1;
    }
    return expect(
        LT_Value_is_boolean(false_value) && !LT_Value_boolean_value(false_value),
        "= false when numbers differ"
    );
}

static int test_numeric_equal_type_error_on_non_number(void){
    LT_Value value = eval_one(
        "(catch :t "
        "  (handler-bind (lambda (c) (throw :t c)) "
        "    (= 1 \"1\")))"
    );

    if (expect(LT_String_p(value), "= non-number signals condition")){
        return 1;
    }
    return expect(
        strcmp(LT_String_value_cstr(LT_String_from_value(value)), "Type error") == 0,
        "= non-number raises type error"
    );
}

static int test_eq_primitive(void){
    LT_Value true_value = eval_one("(eq? 'a 'a)");
    LT_Value false_value = eval_one("(eq? (cons 1 2) (cons 1 2))");

    if (expect(
        LT_Value_is_boolean(true_value) && LT_Value_boolean_value(true_value),
        "eq? true for same identity"
    )){
        return 1;
    }
    return expect(
        LT_Value_is_boolean(false_value) && !LT_Value_boolean_value(false_value),
        "eq? false for distinct objects"
    );
}

static int test_eqv_primitive(void){
    LT_Value value = eval_one("(eqv? 1 1.0)");
    return expect(
        LT_Value_is_boolean(value) && LT_Value_boolean_value(value),
        "eqv? compares numeric values"
    );
}

static int test_equal_primitive(void){
    LT_Value value = eval_one("(equal? '(1 (2 3)) '(1 (2 3)))");
    return expect(
        LT_Value_is_boolean(value) && LT_Value_boolean_value(value),
        "equal? compares nested lists structurally"
    );
}

static int test_not_primitive(void){
    LT_Value nil_value = eval_one("(not ())");
    LT_Value truthy_value = eval_one("(not #false)");

    if (expect(
        LT_Value_is_boolean(nil_value) && LT_Value_boolean_value(nil_value),
        "not true for nil"
    )){
        return 1;
    }
    return expect(
        LT_Value_is_boolean(truthy_value) && !LT_Value_boolean_value(truthy_value),
        "not false for non-nil"
    );
}

static int test_core_type_predicates(void){
    LT_Value null_true = eval_one("(null? ())");
    LT_Value null_false = eval_one("(null? 1)");
    LT_Value boolean_true = eval_one("(boolean? #t)");
    LT_Value number_true = eval_one("(number? 1.5)");
    LT_Value symbol_true = eval_one("(symbol? 'abc)");
    LT_Value primitive_true = eval_one("(primitive? +)");
    LT_Value closure_true = eval_one("(closure? (lambda (x) x))");
    LT_Value macro_true = eval_one("(macro? (macro (lambda (x) x)))");
    LT_Value special_form_true = eval_one("(special-form? if)");
    LT_Value class_true = eval_one("(class? SmallInteger)");
    LT_Value environment_true = eval_one("(environment? (get-current-environment))");

    if (expect(
        LT_Value_is_boolean(null_true) && LT_Value_boolean_value(null_true),
        "null? true for nil"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_boolean(null_false) && !LT_Value_boolean_value(null_false),
        "null? false for non-nil"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_boolean(boolean_true) && LT_Value_boolean_value(boolean_true),
        "boolean? true for booleans"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_boolean(number_true) && LT_Value_boolean_value(number_true),
        "number? true for floats"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_boolean(symbol_true) && LT_Value_boolean_value(symbol_true),
        "symbol? true for symbols"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_boolean(primitive_true) && LT_Value_boolean_value(primitive_true),
        "primitive? true for primitives"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_boolean(closure_true) && LT_Value_boolean_value(closure_true),
        "closure? true for closures"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_boolean(macro_true) && LT_Value_boolean_value(macro_true),
        "macro? true for macros"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_boolean(special_form_true) && LT_Value_boolean_value(special_form_true),
        "special-form? true for special forms"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_boolean(class_true) && LT_Value_boolean_value(class_true),
        "class? true for classes"
    )){
        return 1;
    }
    return expect(
        LT_Value_is_boolean(environment_true)
            && LT_Value_boolean_value(environment_true),
        "environment? true for environments"
    );
}

static int test_list_constructor_primitive(void){
    LT_Value value = eval_one("(list 1 2 3)");

    if (expect(LT_Pair_p(value), "list returns proper list")){
        return 1;
    }
    if (expect(
        LT_Value_is_fixnum(LT_car(value)) && LT_SmallInteger_value(LT_car(value)) == 1,
        "list first element"
    )){
        return 1;
    }
    value = LT_cdr(value);
    if (expect(
        LT_Pair_p(value)
            && LT_Value_is_fixnum(LT_car(value))
            && LT_SmallInteger_value(LT_car(value)) == 2,
        "list second element"
    )){
        return 1;
    }
    value = LT_cdr(value);
    if (expect(
        LT_Pair_p(value)
            && LT_Value_is_fixnum(LT_car(value))
            && LT_SmallInteger_value(LT_car(value)) == 3,
        "list third element"
    )){
        return 1;
    }
    return expect(LT_cdr(value) == LT_NIL, "list terminates with nil");
}

static int test_list_predicate_primitive(void){
    LT_Value true_value = eval_one("(list? '(1 2 3))");
    LT_Value false_value = eval_one("(list? '(1 . 2))");

    if (expect(
        LT_Value_is_boolean(true_value) && LT_Value_boolean_value(true_value),
        "list? true for proper lists"
    )){
        return 1;
    }
    return expect(
        LT_Value_is_boolean(false_value) && !LT_Value_boolean_value(false_value),
        "list? false for dotted pairs"
    );
}

static int test_assoc_primitives(void){
    LT_Value assoc_value = eval_one("(assoc \"ab\" '((\"ab\" . 3) (\"cd\" . 4)))");
    LT_Value assq_value = eval_one("(assq 'b '((a . 1) (b . 2)))");
    LT_Value assq_miss_value = eval_one("(assq (string-append \"a\" \"\") '((\"a\" . 1)))");
    LT_Value assv_value = eval_one("(assv 1.0 '((1 . 9)))");

    if (expect(
        LT_Pair_p(assoc_value)
            && LT_Value_is_fixnum(LT_cdr(assoc_value))
            && LT_SmallInteger_value(LT_cdr(assoc_value)) == 3,
        "assoc matches structurally equal key"
    )){
        return 1;
    }
    if (expect(
        LT_Pair_p(assq_value)
            && LT_Value_is_fixnum(LT_cdr(assq_value))
            && LT_SmallInteger_value(LT_cdr(assq_value)) == 2,
        "assq matches by identity"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_boolean(assq_miss_value)
            && !LT_Value_boolean_value(assq_miss_value),
        "assq returns false when no identity match"
    )){
        return 1;
    }
    return expect(
        LT_Pair_p(assv_value)
            && LT_Value_is_fixnum(LT_cdr(assv_value))
            && LT_SmallInteger_value(LT_cdr(assv_value)) == 9,
        "assv matches numerically equivalent keys"
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

static int test_slot_ref_primitive(void){
    LT_Value value = eval_one("(slot-ref '(1 . 2) 'car)");
    return expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 1,
        "slot-ref reads pair slot"
    );
}

static int test_slot_set_bang_primitive(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value result;

    (void)LT_eval(read_one("(define p (cons 1 2))"), env, NULL);
    (void)LT_eval(read_one("(slot-set! p 'car 77)"), env, NULL);
    result = LT_eval(read_one("(slot-ref p 'car)"), env, NULL);

    return expect(
        LT_Value_is_fixnum(result) && LT_SmallInteger_value(result) == 77,
        "slot-set! mutates pair slot"
    );
}

static int test_slot_table_includes_superclass_slots(void){
    LT_Value class_name = eval_one("(slot-ref (type-of 1) 'name)");

    if (expect(
        LT_Symbol_p(class_name),
        "slot-ref on class object returns inherited Class slot"
    )){
        return 1;
    }
    return expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_value(class_name)), "SmallInteger") == 0,
        "inherited class slot resolves through single lookup"
    );
}

static int test_metaclass_has_valid_name_slot(void){
    LT_Value metaclass_name = eval_one("(slot-ref (type-of SmallInteger) 'name)");

    if (expect(
        LT_Symbol_p(metaclass_name),
        "metaclass name slot contains symbol"
    )){
        return 1;
    }
    return expect(
        strcmp(
            LT_Symbol_name(LT_Symbol_from_value(metaclass_name)),
            "SmallInteger class"
        ) == 0,
        "metaclass gets synthesized class name"
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

static int test_precedence_list_initialized(void){
    LT_Value* precedence_list = LT_SmallInteger_class.precedence_list;

    if (expect(precedence_list != NULL, "class precedence list exists")){
        return 1;
    }
    if (expect(
        precedence_list[0] == LT_STATIC_CLASS(LT_SmallInteger),
        "precedence list starts with class itself"
    )){
        return 1;
    }
    if (expect(
        precedence_list[1] == LT_STATIC_CLASS(LT_Number),
        "precedence list contains direct superclass"
    )){
        return 1;
    }
    if (expect(
        precedence_list[2] == LT_STATIC_CLASS(LT_Object),
        "precedence list contains root object class"
    )){
        return 1;
    }
    return expect(
        precedence_list[3] == LT_INVALID,
        "precedence list is LT_INVALID terminated"
    );
}

static int test_value_is_instance_of_uses_precedence_list(void){
    LT_Value one = LT_SmallInteger_new(1);
    LT_Value small_integer_class = LT_STATIC_CLASS(LT_SmallInteger);

    if (expect(
        LT_Value_is_instance_of(one, small_integer_class),
        "fixnum is instance of SmallInteger"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_instance_of(one, LT_STATIC_CLASS(LT_Number)),
        "fixnum is instance of Number"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_instance_of(one, LT_STATIC_CLASS(LT_Object)),
        "fixnum is instance of Object"
    )){
        return 1;
    }
    if (expect(
        !LT_Value_is_instance_of(one, LT_STATIC_CLASS(LT_Pair)),
        "fixnum is not instance of Pair"
    )){
        return 1;
    }
    return expect(
        LT_Value_is_instance_of(
            small_integer_class,
            LT_STATIC_CLASS(LT_Class)
        ),
        "class object is instance of Class"
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
    failures += test_native_class_lookup();
    failures += test_class_slots_primitive();
    failures += test_send_primitive_uses_direct_method_dictionary();
    failures += test_send_primitive_uses_precedence_lookup_and_cache();
    failures += test_basic_object_and_class_methods();
    failures += test_basic_pair_methods();
    failures += test_basic_string_and_vector_methods();
    failures += test_make_class_primitive();
    failures += test_make_instance_primitive();
    failures += test_class_alloc_method();
    failures += test_make_instance_non_allocatable_class_errors();
    failures += test_class_alloc_non_allocatable_class_errors();
    failures += test_class_add_method_with_selector_method();
    failures += test_class_add_method_invalidates_method_cache();
    failures += test_cons_primitive();
    failures += test_car_primitive();
    failures += test_cdr_primitive();
    failures += test_pair_predicate_primitive();
    failures += test_numeric_equal_primitive();
    failures += test_numeric_equal_type_error_on_non_number();
    failures += test_eq_primitive();
    failures += test_eqv_primitive();
    failures += test_equal_primitive();
    failures += test_not_primitive();
    failures += test_core_type_predicates();
    failures += test_list_constructor_primitive();
    failures += test_list_predicate_primitive();
    failures += test_assoc_primitives();
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
    failures += test_slot_ref_primitive();
    failures += test_slot_set_bang_primitive();
    failures += test_slot_table_includes_superclass_slots();
    failures += test_metaclass_has_valid_name_slot();
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
    failures += test_precedence_list_initialized();
    failures += test_value_is_instance_of_uses_precedence_list();
    failures += test_boolean_constants();
    failures += test_character_api_uses_unicode_codepoints();

    if (failures == 0){
        puts("eval tests passed");
        return 0;
    }

    fprintf(stderr, "%d eval test(s) failed\n", failures);
    return 1;
}
