/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Boolean.h>
#include <ListTalk/classes/Character.h>
#include <ListTalk/classes/Condition.h>
#include <ListTalk/classes/Float.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Package.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Reader.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/classes/Vector.h>
#include <ListTalk/macros/arg_macros.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

static const char* condition_message_cstr(LT_Value condition){
    LT_Value message = LT_Object_slot_ref(condition, LT_Symbol_new("message"));
    return LT_String_value_cstr(LT_String_from_value(message));
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

LT_DEFINE_PRIMITIVE(
    primitive_test_invocation_context_kind_method,
    "test-invocation-context-kind-method",
    "(self)",
    "Test helper method: return invocation context kind."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)self;
    (void)invocation_context_data;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return invocation_context_kind;
}

LT_DEFINE_PRIMITIVE(
    primitive_test_invocation_context_data_method,
    "test-invocation-context-data-method",
    "(self)",
    "Test helper method: return invocation context data."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)self;
    (void)invocation_context_kind;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return invocation_context_data;
}

LT_DEFINE_PRIMITIVE(
    primitive_test_small_integer_marker_method,
    "test-small-integer-marker-method",
    "(self)",
    "Test helper method: return SmallInteger marker."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)self;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Symbol_new("SmallIntegerMarker");
}

LT_DEFINE_PRIMITIVE(
    primitive_test_integer_marker_method,
    "test-integer-marker-method",
    "(self)",
    "Test helper method: return Integer marker."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)self;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Symbol_new("IntegerMarker");
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

static int string_starts_with(const char* value, const char* prefix){
    size_t prefix_len = strlen(prefix);
    return strncmp(value, prefix, prefix_len) == 0;
}

static char* debug_string_for_value(LT_Value value){
    char* buffer = NULL;
    size_t size = 0;
    FILE* stream = open_memstream(&buffer, &size);

    if (stream == NULL){
        fail("open_memstream failed");
        return NULL;
    }
    LT_Value_debugPrintOn(value, stream);
    fclose(stream);
    return buffer;
}

static const char* os_module_resolver_path(void){
    if (access("os.ltm", R_OK) == 0){
        return ".";
    }
    if (access("builddir/os.ltm", R_OK) == 0){
        return "builddir";
    }
    return NULL;
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
    char* printed;

    if (expect(LT_Value_class(value) == &LT_SmallFraction_class, "integer division returns fraction")){
        return 1;
    }
    printed = debug_string_for_value(value);
    return expect(
        strcmp(printed, "5/2") == 0,
        "integer division remains exact"
    );
}

static int test_fixnum_overflow_promotes_to_big_integer(void){
    LT_Value value = eval_one("(+ 340282366920938463463374607431768211456 1)");
    char* printed;

    if (expect(LT_Value_class(value) == &LT_BigInteger_class, "overflow promotes to big integer")){
        return 1;
    }
    printed = debug_string_for_value(value);
    return expect(
        strcmp(printed, "340282366920938463463374607431768211457") == 0,
        "big integer value"
    );
}

static int test_fraction_addition_is_reduced(void){
    LT_Value value = eval_one("(+ 1/2 1/3)");

    return expect(
        LT_Value_class(value) == &LT_SmallFraction_class
            && LT_SmallFraction_numerator(value) == 5
            && LT_SmallFraction_denominator(value) == 6,
        "fraction addition stays reduced"
    );
}

static int test_big_integer_multiplication_beyond_int128(void){
    LT_Value value = eval_one(
        "(* 18446744073709551616 18446744073709551616)"
    );
    char* printed;

    if (expect(LT_Value_class(value) == &LT_BigInteger_class, "large multiplication returns big integer")){
        return 1;
    }
    printed = debug_string_for_value(value);
    return expect(
        strcmp(printed, "340282366920938463463374607431768211456") == 0,
        "large multiplication value"
    );
}

static int test_complex_addition(void){
    LT_Value value = eval_one("(+ 1+2i 3+4i)");
    char* printed = debug_string_for_value(value);
    return expect(strcmp(printed, "4+6i") == 0, "complex addition");
}

static int test_complex_multiplication(void){
    LT_Value value = eval_one("(* 1+2i 3+4i)");
    char* printed = debug_string_for_value(value);
    return expect(strcmp(printed, "-5+10i") == 0, "complex multiplication");
}

static int test_complex_division(void){
    LT_Value value = eval_one("(/ 1+2i 3+4i)");
    char* printed = debug_string_for_value(value);
    return expect(strcmp(printed, "11/25+2/25i") == 0, "complex division");
}

static int test_real_complex_mixed_addition(void){
    LT_Value value = eval_one("(+ 2 1+3i)");
    char* printed = debug_string_for_value(value);
    return expect(strcmp(printed, "3+3i") == 0, "real and complex mixed addition");
}

static int test_fraction_multiplication_canonicalizes_to_integer(void){
    LT_Value value = eval_one("(* 2 3/2)");
    return expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 3,
        "fraction multiplication canonicalizes to integer"
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

static int test_environment_invocation_context_lookup_walks_parent_frames(void){
    LT_Environment* root = LT_Environment_new(NULL, LT_NIL, LT_NIL);
    LT_Value kind = (LT_Value)(uintptr_t)&LT_send_invocation_context;
    LT_Value data = LT_Symbol_new("send-context-data");
    LT_Environment* middle = LT_Environment_new(root, kind, data);
    LT_Environment* leaf = LT_Environment_new(middle, LT_NIL, LT_NIL);

    if (expect(
        LT_Environment_invocation_context_of_kind(leaf, kind) == data,
        "environment lookup finds invocation context in parent frame"
    )){
        return 1;
    }
    return expect(
        LT_Environment_invocation_context_of_kind(leaf, LT_NIL) == LT_NIL,
        "environment lookup returns nil when invocation context kind is absent"
    );
}

static int test_send_passes_invocation_context_kind_to_primitive_method(void){
    LT_Value selector = LT_Symbol_new_in(LT_PACKAGE_KEYWORD, "invocation-context-kind");
    LT_Value result;

    LT_Class_addMethod(
        &LT_Object_class,
        selector,
        LT_Primitive_from_static(&primitive_test_invocation_context_kind_method)
    );

    result = LT_send(LT_SmallInteger_new(1), selector, LT_NIL, NULL);
    return expect(
        result == (LT_Value)(uintptr_t)&LT_send_invocation_context,
        "send passes send invocation context kind to method"
    );
}

static int test_send_passes_next_precedence_tail_as_invocation_context_data(void){
    LT_Value selector = LT_Symbol_new_in(LT_PACKAGE_KEYWORD, "next-precedence-tail");
    LT_Value result;

    LT_Class_addMethod(
        &LT_Integer_class,
        selector,
        LT_Primitive_from_static(&primitive_test_invocation_context_data_method)
    );

    result = LT_send(LT_SmallInteger_new(1), selector, LT_NIL, NULL);
    if (expect(
        LT_ImmutableList_p(result),
        "send passes immutable precedence tail as invocation context data"
    )){
        return 1;
    }
    return expect(
        LT_ImmutableList_car(result) == LT_STATIC_CLASS(LT_RationalNumber),
        "send invocation context data begins after matched class"
    );
}

static int test_super_send_c_api_uses_explicit_precedence_list(void){
    LT_Value selector = LT_Symbol_new_in(LT_PACKAGE_KEYWORD, "super-marker");
    LT_Value precedence_tail = LT_ImmutableList_cdr(
        LT_Class_precedence_list(&LT_SmallInteger_class)
    );
    LT_Value result;

    LT_Class_addMethod(
        &LT_SmallInteger_class,
        selector,
        LT_Primitive_from_static(&primitive_test_small_integer_marker_method)
    );
    LT_Class_addMethod(
        &LT_Integer_class,
        selector,
        LT_Primitive_from_static(&primitive_test_integer_marker_method)
    );

    result = LT_super_send(
        LT_SmallInteger_new(1),
        precedence_tail,
        selector,
        LT_NIL,
        NULL
    );
    return expect(
        LT_Symbol_p(result)
            && strcmp(LT_Symbol_name(LT_Symbol_from_value(result)), "IntegerMarker") == 0,
        "LT_super_send starts lookup from explicit precedence tail"
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

    if (expect(LT_Error_p(value), "make-instance on Pair signals condition")){
        return 1;
    }
    return expect(
        strcmp(condition_message_cstr(value), "Class is not allocatable") == 0,
        "make-instance on non-allocatable class raises error"
    );
}

static int test_class_alloc_non_allocatable_class_errors(void){
    LT_Value value = eval_one(
        "(catch :t "
        "  (handler-bind (lambda (c) (throw :t c)) "
        "    [Pair alloc]))"
    );

    if (expect(LT_Error_p(value), "Class>>alloc on Pair signals condition")){
        return 1;
    }
    return expect(
        strcmp(condition_message_cstr(value), "Class is not allocatable") == 0,
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

    if (expect(LT_Error_p(value), "= non-number signals condition")){
        return 1;
    }
    return expect(
        strcmp(condition_message_cstr(value), "Type error") == 0,
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
    LT_Value false_value = eval_one("(not #false)");

    if (expect(
        LT_Value_is_boolean(nil_value) && LT_Value_boolean_value(nil_value),
        "not true for nil"
    )){
        return 1;
    }
    return expect(
        LT_Value_is_boolean(false_value) && LT_Value_boolean_value(false_value),
        "not true for #false"
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

static int test_immutable_list_interops_with_pairs(void){
    LT_Value values[] = {
        LT_SmallInteger_new(1),
        LT_SmallInteger_new(2),
    };
    LT_Value immutable_list = LT_ImmutableList_new(2, values);
    LT_Value dotted_immutable = LT_ImmutableList_new_with_tail(
        2,
        values,
        LT_SmallInteger_new(3)
    );
    LT_Value pair_list = LT_cons(
        LT_SmallInteger_new(1),
        LT_cons(LT_SmallInteger_new(2), LT_NIL)
    );
    LT_Value dotted_pair = LT_cons(
        LT_SmallInteger_new(1),
        LT_cons(LT_SmallInteger_new(2), LT_SmallInteger_new(3))
    );

    if (expect(
        LT_Value_is_fixnum(LT_car(immutable_list))
            && LT_SmallInteger_value(LT_car(immutable_list)) == 1,
        "immutable list car returns first item"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_fixnum(LT_car(LT_cdr(immutable_list)))
            && LT_SmallInteger_value(LT_car(LT_cdr(immutable_list))) == 2,
        "immutable list cdr advances within compact storage"
    )){
        return 1;
    }
    if (expect(LT_cdr(LT_cdr(immutable_list)) == LT_NIL, "immutable list ends with nil")){
        return 1;
    }
    if (expect(
        LT_Value_is_fixnum(LT_cdr(LT_cdr(dotted_immutable)))
            && LT_SmallInteger_value(LT_cdr(LT_cdr(dotted_immutable))) == 3,
        "immutable list dotted tail preserved"
    )){
        return 1;
    }
    if (expect(LT_Value_equal_p(immutable_list, pair_list), "pair equals immutable list")){
        return 1;
    }
    if (expect(LT_Value_equal_p(dotted_immutable, dotted_pair), "dotted pair equals immutable list")){
        return 1;
    }
    return expect(
        LT_Value_hash(immutable_list) == LT_Value_hash(pair_list),
        "equal pair and immutable list share hash"
    );
}

static int test_immutable_list_methods(void){
    LT_Value values[] = {
        LT_SmallInteger_new(4),
        LT_SmallInteger_new(5),
    };
    LT_Value immutable_list = LT_ImmutableList_new(2, values);
    LT_Value selector_car = LT_Symbol_new_in(LT_PACKAGE_KEYWORD, "car");
    LT_Value selector_cdr = LT_Symbol_new_in(LT_PACKAGE_KEYWORD, "cdr");
    LT_Value car_value = LT_send(immutable_list, selector_car, LT_NIL, NULL);
    LT_Value cdr_value = LT_send(immutable_list, selector_cdr, LT_NIL, NULL);

    if (expect(
        LT_Value_is_fixnum(car_value) && LT_SmallInteger_value(car_value) == 4,
        "ImmutableList>>car dispatches correctly"
    )){
        return 1;
    }
    return expect(
        LT_Value_is_fixnum(LT_car(cdr_value))
            && LT_SmallInteger_value(LT_car(cdr_value)) == 5,
        "ImmutableList>>cdr dispatches correctly"
    );
}

static int test_immutable_list_from_list(void){
    LT_Value proper_list = LT_cons(
        LT_SmallInteger_new(6),
        LT_cons(LT_SmallInteger_new(7), LT_NIL)
    );
    LT_Value dotted_list = LT_cons(
        LT_SmallInteger_new(8),
        LT_cons(LT_SmallInteger_new(9), LT_SmallInteger_new(10))
    );
    LT_Value converted = LT_ImmutableList_fromList(proper_list);
    LT_Value dotted_converted = LT_ImmutableList_fromList(dotted_list);
    LT_Value class_converted = eval_one("[ImmutableList fromList: '(11 12)]");

    if (expect(LT_ImmutableList_p(converted), "LT_ImmutableList_fromList returns immutable list")){
        return 1;
    }
    if (expect(
        LT_Value_is_fixnum(LT_car(converted))
            && LT_SmallInteger_value(LT_car(converted)) == 6,
        "LT_ImmutableList_fromList preserves first element"
    )){
        return 1;
    }
    if (expect(
        LT_cdr(LT_cdr(converted)) == LT_NIL,
        "LT_ImmutableList_fromList preserves proper list terminator"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_fixnum(LT_cdr(LT_cdr(dotted_converted)))
            && LT_SmallInteger_value(LT_cdr(LT_cdr(dotted_converted))) == 10,
        "LT_ImmutableList_fromList preserves dotted tail"
    )){
        return 1;
    }
    if (expect(LT_ImmutableList_p(class_converted), "ImmutableList class>>fromList: returns immutable list")){
        return 1;
    }
    return expect(
        LT_Value_is_fixnum(LT_car(LT_cdr(class_converted)))
            && LT_SmallInteger_value(LT_car(LT_cdr(class_converted))) == 12,
        "ImmutableList class>>fromList: preserves list contents"
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

static int test_append_primitive(void){
    LT_Value value = eval_one("(append '(1 2) '(3 4))");

    if (expect(
        LT_Value_is_fixnum(LT_car(value))
            && LT_SmallInteger_value(LT_car(value)) == 1,
        "append preserves first element"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_fixnum(LT_car(LT_cdr(LT_cdr(value))))
            && LT_SmallInteger_value(LT_car(LT_cdr(LT_cdr(value)))) == 3,
        "append includes later list elements"
    )){
        return 1;
    }
    return expect(
        LT_Value_is_fixnum(LT_car(LT_cdr(LT_cdr(LT_cdr(value)))))
            && LT_SmallInteger_value(LT_car(LT_cdr(LT_cdr(LT_cdr(value))))) == 4,
        "append reaches final element"
    );
}

static int test_memq_primitive(void){
    LT_Value hit = eval_one("(memq 'b '(a b c))");
    LT_Value miss = eval_one("(memq (string-append \"a\" \"\") '(\"a\"))");

    if (expect(
        LT_Pair_p(hit)
            && LT_Symbol_p(LT_car(hit))
            && strcmp(LT_Symbol_name(LT_Symbol_from_value(LT_car(hit))), "b") == 0,
        "memq returns matching tail"
    )){
        return 1;
    }
    return expect(
        LT_Value_is_boolean(miss) && !LT_Value_boolean_value(miss),
        "memq returns false on identity miss"
    );
}

static int test_cxxxr_primitives(void){
    LT_Value caar_value = eval_one("(caar '((1 . 2) . 3))");
    LT_Value cadr_value = eval_one("(cadr '(1 2 3))");
    LT_Value caddr_value = eval_one("(caddr '(1 2 3 4))");
    LT_Value cadddr_value = eval_one("(cadddr '(1 2 3 4 5))");

    if (expect(
        LT_Value_is_fixnum(caar_value) && LT_SmallInteger_value(caar_value) == 1,
        "caar returns nested car"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_fixnum(cadr_value) && LT_SmallInteger_value(cadr_value) == 2,
        "cadr returns second element"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_fixnum(caddr_value) && LT_SmallInteger_value(caddr_value) == 3,
        "caddr returns third element"
    )){
        return 1;
    }
    return expect(
        LT_Value_is_fixnum(cadddr_value) && LT_SmallInteger_value(cadddr_value) == 4,
        "cadddr returns fourth element"
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

static int test_quasiquote_unquote(void){
    LT_Value value = eval_one("(quasiquote (+ (unquote (+ 1 2)) 4))");

    if (expect(LT_Pair_p(value), "quasiquote returns list")){
        return 1;
    }
    if (expect(
        LT_Symbol_p(LT_car(value))
            && strcmp(LT_Symbol_name(LT_Symbol_from_value(LT_car(value))), "+") == 0,
        "quasiquote keeps literal symbol"
    )){
        return 1;
    }
    value = LT_cdr(value);
    if (expect(
        LT_Pair_p(value)
            && LT_Value_is_fixnum(LT_car(value))
            && LT_SmallInteger_value(LT_car(value)) == 3,
        "quasiquote evaluates unquote expression"
    )){
        return 1;
    }
    value = LT_cdr(value);
    if (expect(
        LT_Pair_p(value)
            && LT_Value_is_fixnum(LT_car(value))
            && LT_SmallInteger_value(LT_car(value)) == 4,
        "quasiquote keeps trailing literal"
    )){
        return 1;
    }
    return expect(LT_cdr(value) == LT_NIL, "quasiquote list terminates");
}

static int test_quasiquote_reader_syntax(void){
    LT_Value value = eval_one("`(+ ,(+ 1 2) 4)");

    if (expect(LT_Pair_p(value), "quasiquote reader returns list")){
        return 1;
    }
    if (expect(
        LT_Symbol_p(LT_car(value))
            && strcmp(LT_Symbol_name(LT_Symbol_from_value(LT_car(value))), "+") == 0,
        "quasiquote reader keeps literal symbol"
    )){
        return 1;
    }
    value = LT_cdr(value);
    if (expect(
        LT_Pair_p(value)
            && LT_Value_is_fixnum(LT_car(value))
            && LT_SmallInteger_value(LT_car(value)) == 3,
        "quasiquote reader evaluates comma"
    )){
        return 1;
    }
    value = LT_cdr(value);
    if (expect(
        LT_Pair_p(value)
            && LT_Value_is_fixnum(LT_car(value))
            && LT_SmallInteger_value(LT_car(value)) == 4,
        "quasiquote reader keeps literal tail"
    )){
        return 1;
    }
    return expect(LT_cdr(value) == LT_NIL, "quasiquote reader list terminates");
}

static int test_quasiquote_unquote_splicing(void){
    LT_Value value = eval_one("`(1 ,@(list 2 3) 4)");

    if (expect(LT_Pair_p(value), "unquote-splicing returns list")){
        return 1;
    }
    if (expect(
        LT_Value_is_fixnum(LT_car(value)) && LT_SmallInteger_value(LT_car(value)) == 1,
        "unquote-splicing first element"
    )){
        return 1;
    }
    value = LT_cdr(value);
    if (expect(
        LT_Pair_p(value)
            && LT_Value_is_fixnum(LT_car(value))
            && LT_SmallInteger_value(LT_car(value)) == 2,
        "unquote-splicing inserts first spliced item"
    )){
        return 1;
    }
    value = LT_cdr(value);
    if (expect(
        LT_Pair_p(value)
            && LT_Value_is_fixnum(LT_car(value))
            && LT_SmallInteger_value(LT_car(value)) == 3,
        "unquote-splicing inserts second spliced item"
    )){
        return 1;
    }
    value = LT_cdr(value);
    if (expect(
        LT_Pair_p(value)
            && LT_Value_is_fixnum(LT_car(value))
            && LT_SmallInteger_value(LT_car(value)) == 4,
        "unquote-splicing keeps trailing element"
    )){
        return 1;
    }
    return expect(LT_cdr(value) == LT_NIL, "unquote-splicing list terminates");
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
    LT_Value nil_branch = eval_one("(if () 1 2)");
    LT_Value false_branch = eval_one("(if #false 1 2)");

    if (expect(
        LT_Value_is_fixnum(nil_branch) && LT_SmallInteger_value(nil_branch) == 2,
        "if nil branch is false"
    )){
        return 1;
    }
    return expect(
        LT_Value_is_fixnum(false_branch) && LT_SmallInteger_value(false_branch) == 2,
        "if #false branch is false"
    );
}

static int test_and_special_form(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value value;

    value = LT_eval(read_one("(and)"), env, NULL);
    if (expect(value == LT_TRUE, "and with no expressions returns true")){
        return 1;
    }

    value = LT_eval(read_one("(and 1 2 3)"), env, NULL);
    if (expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 3,
        "and returns last truthy value"
    )){
        return 1;
    }

    value = LT_eval(read_one("(and 1 () (error \"skipped\"))"), env, NULL);
    if (expect(value == LT_NIL, "and returns first falsey value")){
        return 1;
    }

    (void)LT_eval(read_one("(define x 0)"), env, NULL);
    value = LT_eval(
        read_one("(let () (and #false (set! x 1)) x)"),
        env,
        NULL
    );
    if (expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 0,
        "and short-circuits after falsey value"
    )){
        return 1;
    }

    (void)LT_eval(read_one("(and #true (define and-defined 23))"), env, NULL);
    value = LT_eval(read_one("and-defined"), env, NULL);
    return expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 23,
        "and does not wrap later forms in a lexical environment"
    );
}

static int test_or_special_form(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value value;

    value = LT_eval(read_one("(or)"), env, NULL);
    if (expect(value == LT_FALSE, "or with no expressions returns false")){
        return 1;
    }

    value = LT_eval(read_one("(or () #false 7 (error \"skipped\"))"), env, NULL);
    if (expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 7,
        "or returns first truthy value"
    )){
        return 1;
    }

    value = LT_eval(read_one("(or () #false)"), env, NULL);
    if (expect(value == LT_FALSE, "or returns last falsey value")){
        return 1;
    }

    (void)LT_eval(read_one("(define x 0)"), env, NULL);
    value = LT_eval(
        read_one("(let () (or 5 (set! x 1)) x)"),
        env,
        NULL
    );
    if (expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 0,
        "or short-circuits after truthy value"
    )){
        return 1;
    }

    (void)LT_eval(read_one("(define foo #false)"), env, NULL);
    (void)LT_eval(read_one("(or foo (define or-defined 24))"), env, NULL);
    value = LT_eval(read_one("or-defined"), env, NULL);
    return expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 24,
        "or does not wrap later forms in a lexical environment"
    );
}

static int test_cond_special_form(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value value;

    value = LT_eval(
        read_one("(cond (() (error \"skipped\")) ((= 1 1) 42) (else 99))"),
        env,
        NULL
    );
    if (expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 42,
        "cond evaluates first truthy clause body"
    )){
        return 1;
    }

    value = LT_eval(read_one("(cond (() 1) (else 2 3))"), env, NULL);
    if (expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 3,
        "cond else clause evaluates body sequence"
    )){
        return 1;
    }

    value = LT_eval(read_one("(cond ((+ 1 2)))"), env, NULL);
    if (expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 3,
        "cond clause without body returns test value"
    )){
        return 1;
    }

    value = LT_eval(read_one("(cond (() 1))"), env, NULL);
    if (expect(value == LT_FALSE, "cond without matching clause returns false")){
        return 1;
    }

    (void)LT_eval(read_one("(cond (#true (define cond-defined 25)))"), env, NULL);
    value = LT_eval(read_one("cond-defined"), env, NULL);
    return expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 25,
        "cond does not wrap clause bodies in a lexical environment"
    );
}

static int test_begin_special_form(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value value;

    value = LT_eval(read_one("(begin)"), env, NULL);
    if (expect(value == LT_NIL, "begin with no forms returns nil")){
        return 1;
    }

    value = LT_eval(read_one("(begin 1 2 3)"), env, NULL);
    if (expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 3,
        "begin returns last body value"
    )){
        return 1;
    }

    (void)LT_eval(read_one("(begin (define begin-defined 26))"), env, NULL);
    value = LT_eval(read_one("begin-defined"), env, NULL);
    return expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 26,
        "begin evaluates body in current environment"
    );
}

static int test_let_special_form(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value value;

    value = LT_eval(read_one("(let ((x 1) (y 2)) (+ x y))"), env, NULL);
    if (expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 3,
        "let binds local names for body evaluation"
    )){
        return 1;
    }

    (void)LT_eval(read_one("(define x 10)"), env, NULL);
    value = LT_eval(read_one("(let ((x 1) (y x)) y)"), env, NULL);
    if (expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 10,
        "let evaluates binding expressions in outer environment"
    )){
        return 1;
    }

    value = LT_eval(read_one("(let ((x 99)) x)"), env, NULL);
    if (expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 99,
        "let local binding shadows outer binding in body"
    )){
        return 1;
    }

    value = LT_eval(read_one("x"), env, NULL);
    return expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 10,
        "let local binding does not leak to outer environment"
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

static int test_define_function_shorthand(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value result;

    (void)LT_eval(read_one("(define (add2 x y) (+ x y))"), env, NULL);
    result = LT_eval(read_one("(add2 3 4)"), env, NULL);
    if (expect(
        LT_Value_is_fixnum(result) && LT_SmallInteger_value(result) == 7,
        "define function shorthand creates callable binding"
    )){
        return 1;
    }

    (void)LT_eval(read_one("(define (bump x) (set! x (+ x 1)) x)"), env, NULL);
    result = LT_eval(read_one("(bump 9)"), env, NULL);
    return expect(
        LT_Value_is_fixnum(result) && LT_SmallInteger_value(result) == 10,
        "define function shorthand keeps full function body sequence"
    );
}

static int test_define_function_shorthand_is_constant(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value value;

    (void)LT_eval(read_one("(define (id x) x)"), env, NULL);
    value = LT_eval(
        read_one(
            "(catch :t "
            "  (handler-bind (lambda (c) (throw :t c)) "
            "    (set! id (lambda (x) x))))"
        ),
        env,
        NULL
    );

    if (expect(LT_Error_p(value), "set! on function shorthand signals condition")){
        return 1;
    }
    return expect(
        strcmp(
            condition_message_cstr(value),
            "Special form set! expected existing mutable binding"
        ) == 0,
        "define function shorthand creates constant binding"
    );
}

static int test_define_macro_shorthand(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value result;

    (void)LT_eval(
        read_one("(define-macro (inc x) (list '+ x 1))"),
        env,
        NULL
    );
    result = LT_eval(read_one("(inc 41)"), env, NULL);
    return expect(
        LT_Value_is_fixnum(result) && LT_SmallInteger_value(result) == 42,
        "define-macro shorthand defines macro with lambda-style parameters"
    );
}

static int test_define_macro_shorthand_is_constant(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value value;

    (void)LT_eval(read_one("(define-macro (m x) x)"), env, NULL);
    value = LT_eval(
        read_one(
            "(catch :t "
            "  (handler-bind (lambda (c) (throw :t c)) "
            "    (set! m (macro (lambda (x) x)))))"
        ),
        env,
        NULL
    );

    if (expect(LT_Error_p(value), "set! on define-macro binding signals condition")){
        return 1;
    }
    return expect(
        strcmp(
            condition_message_cstr(value),
            "Special form set! expected existing mutable binding"
        ) == 0,
        "define-macro creates constant binding"
    );
}

static int test_lambda_macro_expands_to_named_nil_closure(void){
    LT_Value value = eval_one("(lambda (x) x)");
    LT_Closure* closure;

    if (expect(LT_Closure_p(value), "lambda returns closure via macro expansion")){
        return 1;
    }
    closure = LT_Closure_from_value(value);
    return expect(
        LT_Closure_name(closure) == LT_NIL,
        "lambda macro expands to %lambda with nil name"
    );
}

static int test_define_function_shorthand_sets_closure_name(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value value;
    LT_Closure* closure;

    (void)LT_eval(read_one("(define (named-add x y) (+ x y))"), env, NULL);
    value = LT_eval(read_one("named-add"), env, NULL);
    if (expect(LT_Closure_p(value), "define shorthand binds closure value")){
        return 1;
    }
    closure = LT_Closure_from_value(value);
    return expect(
        LT_Symbol_p(LT_Closure_name(closure))
            && strcmp(
                LT_Symbol_name(LT_Symbol_from_value(LT_Closure_name(closure))),
                "named-add"
            ) == 0,
        "define shorthand preserves function name on closure"
    );
}

static int test_closure_debug_print_includes_name(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value value;
    char* printed;
    int result;

    (void)LT_eval(read_one("(define (named-debug x) x)"), env, NULL);
    value = LT_eval(read_one("named-debug"), env, NULL);
    printed = debug_string_for_value(value);
    if (printed == NULL){
        return 1;
    }

    result = expect(
        strcmp(printed, "#<Closure named-debug>") == 0,
        "closure debug print includes name"
    );
    free(printed);
    return result;
}

static int test_anonymous_closure_debug_print_includes_address(void){
    LT_Value value = eval_one("(lambda (x) x)");
    char* printed = debug_string_for_value(value);
    int result;

    if (printed == NULL){
        return 1;
    }

    result = expect(
        string_starts_with(printed, "#<Closure at 0x"),
        "anonymous closure debug print includes address"
    );
    free(printed);
    return result;
}

static int test_symbol_uninterned_and_gensym_c_api(void){
    LT_Value uninterned = LT_Symbol_new_uninterned("temp");
    LT_Value generated = LT_Symbol_gensym(NULL);
    LT_Value named_generated = LT_Symbol_gensym("named");
    char* uninterned_debug = NULL;
    char* generated_debug = NULL;
    char* named_generated_debug = NULL;

    if (expect(LT_Symbol_p(uninterned), "LT_Symbol_new_uninterned returns symbol")){
        return 1;
    }
    if (expect(
        LT_Symbol_package(LT_Symbol_from_value(uninterned)) == NULL,
        "uninterned symbol has nil package"
    )){
        return 1;
    }
    if (expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_value(uninterned)), "temp") == 0,
        "uninterned symbol keeps exact name"
    )){
        return 1;
    }
    if (expect(LT_Symbol_p(generated), "LT_Symbol_gensym returns symbol")){
        return 1;
    }
    if (expect(
        LT_Symbol_name(LT_Symbol_from_value(generated)) == NULL,
        "opaque gensym stores NULL name"
    )){
        return 1;
    }
    if (expect(
        LT_Symbol_package(LT_Symbol_from_value(generated)) == NULL,
        "gensym symbol has nil package"
    )){
        return 1;
    }
    uninterned_debug = debug_string_for_value(uninterned);
    generated_debug = debug_string_for_value(generated);
    named_generated_debug = debug_string_for_value(named_generated);
    if (uninterned_debug == NULL
        || generated_debug == NULL
        || named_generated_debug == NULL){
        return 1;
    }
    if (expect(
        strcmp(uninterned_debug, "#:temp") == 0,
        "named uninterned symbol prints #:name"
    )){
        return 1;
    }
    if (expect(
        string_starts_with(generated_debug, "#<gensym at 0x"),
        "gensym prints opaque identity form"
    )){
        return 1;
    }
    return expect(
        strcmp(named_generated_debug, "#:named") == 0,
        "named gensym behaves like uninterned symbol"
    );
}

static int test_gensym_primitive(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value first;
    LT_Value second;
    LT_Value named_from_string;
    LT_Value named_from_symbol;
    char* first_debug = NULL;
    char* second_debug = NULL;
    char* named_from_string_debug = NULL;
    char* named_from_symbol_debug = NULL;

    first = LT_eval(read_one("(gensym)"), env, NULL);
    second = LT_eval(read_one("(gensym)"), env, NULL);
    named_from_string = LT_eval(read_one("(gensym \"tmp\")"), env, NULL);
    named_from_symbol = LT_eval(read_one("(gensym 'symtmp)"), env, NULL);

    if (expect(LT_Symbol_p(first), "gensym primitive returns symbol")){
        return 1;
    }
    if (expect(
        LT_Symbol_name(LT_Symbol_from_value(first)) == NULL,
        "gensym primitive unnamed form has NULL name"
    )){
        return 1;
    }
    if (expect(
        LT_Symbol_package(LT_Symbol_from_value(first)) == NULL,
        "gensym primitive returns uninterned symbol"
    )){
        return 1;
    }
    if (expect(first != second, "gensym primitive returns unique symbols")){
        return 1;
    }
    first_debug = debug_string_for_value(first);
    second_debug = debug_string_for_value(second);
    named_from_string_debug = debug_string_for_value(named_from_string);
    named_from_symbol_debug = debug_string_for_value(named_from_symbol);
    if (first_debug == NULL
        || second_debug == NULL
        || named_from_string_debug == NULL
        || named_from_symbol_debug == NULL){
        return 1;
    }
    if (expect(
        string_starts_with(first_debug, "#<gensym at 0x"),
        "gensym primitive print form is opaque identity"
    )){
        return 1;
    }
    if (expect(
        strcmp(first_debug, second_debug) != 0,
        "gensym print forms differ for distinct symbols"
    )){
        return 1;
    }
    if (expect(
        strcmp(named_from_string_debug, "#:tmp") == 0,
        "gensym accepts string name designator"
    )){
        return 1;
    }
    return expect(
        strcmp(named_from_symbol_debug, "#:symtmp") == 0,
        "gensym accepts symbol name designator"
    );
}

static int test_symbol_class_methods_for_uninterned_and_gensym(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value generated = LT_eval(read_one("[Symbol gensym]"), env, NULL);
    LT_Value uninterned = LT_eval(read_one("[Symbol uninterned: 'temp]"), env, NULL);

    if (expect(LT_Symbol_p(generated), "Symbol class>>gensym returns symbol")){
        return 1;
    }
    if (expect(
        LT_Symbol_package(LT_Symbol_from_value(generated)) == NULL,
        "Symbol class>>gensym returns uninterned symbol"
    )){
        return 1;
    }
    if (expect(
        LT_Symbol_p(uninterned),
        "Symbol class>>uninterned: returns symbol"
    )){
        return 1;
    }
    if (expect(
        LT_Symbol_package(LT_Symbol_from_value(uninterned)) == NULL,
        "Symbol class>>uninterned: returns uninterned symbol"
    )){
        return 1;
    }
    return expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_value(uninterned)), "temp") == 0,
        "Symbol class>>uninterned: accepts symbol name designator"
    );
}

static int test_symbol_name_method(void){
    LT_Value value = eval_one("['sample name]");

    return expect(
        LT_String_p(value)
            && strcmp(LT_String_value_cstr(LT_String_from_value(value)), "sample") == 0,
        "Symbol>>name returns symbol name string"
    );
}

static int test_with_gensyms_macro(void){
    LT_Value result = eval_one(
        "(with-gensyms (a b) "
        "  (list (symbol? a) "
        "        (symbol? b) "
        "        (eq? a b) "
        "        (null? (slot-ref a 'package)) "
        "        (null? (slot-ref b 'package))))"
    );

    if (expect(LT_Pair_p(result), "with-gensyms returns list in test harness")){
        return 1;
    }
    if (expect(
        LT_Value_boolean_value(LT_car(result)),
        "with-gensyms binds symbol for first name"
    )){
        return 1;
    }
    result = LT_cdr(result);
    if (expect(
        LT_Value_boolean_value(LT_car(result)),
        "with-gensyms binds symbol for second name"
    )){
        return 1;
    }
    result = LT_cdr(result);
    if (expect(
        !LT_Value_boolean_value(LT_car(result)),
        "with-gensyms symbols are distinct"
    )){
        return 1;
    }
    result = LT_cdr(result);
    if (expect(
        LT_Value_boolean_value(LT_car(result)),
        "with-gensyms first symbol package is nil"
    )){
        return 1;
    }
    result = LT_cdr(result);
    return expect(
        LT_Value_boolean_value(LT_car(result)),
        "with-gensyms second symbol package is nil"
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

static int test_set_bang_macro_dispatches_self_slot(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value result;

    (void)LT_eval(
        read_one("(define-method [Pair setFirst: value] (set! (%self-slot car) value) value)"),
        env,
        NULL
    );
    result = LT_eval(read_one("['(1 . 2) setFirst: 77]"), env, NULL);

    return expect(
        LT_Value_is_fixnum(result) && LT_SmallInteger_value(result) == 77,
        "set! macro dispatches %self-slot target to slot-set!"
    );
}

static int test_define_class_macro(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value instance;

    (void)LT_eval(
        read_one("(define-class Point (Object) (x y))"),
        env,
        NULL
    );
    instance = LT_eval(read_one("(make-instance Point)"), env, NULL);

    return expect(
        LT_Value_class(instance) == LT_Class_from_object(
            LT_eval(read_one("Point"), env, NULL)
        ),
        "define-class wraps make-class and define"
    );
}

static int test_define_method_macro(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value result;
    LT_Value method;
    LT_Value method_name;
    LT_Value cursor;

    (void)LT_eval(
        read_one("(define-method [Pair first] (%self-slot car))"),
        env,
        NULL
    );
    result = LT_eval(read_one("['(9 . 8) first]"), env, NULL);
    if (expect(
        LT_Value_is_fixnum(result) && LT_SmallInteger_value(result) == 9,
        "define-method installs zero-argument method"
    )){
        return 1;
    }

    method = LT_eval(read_one("[Pair lookupSelector: :first]"), env, NULL);
    if (expect(LT_Closure_p(method), "define-method installs closure method")){
        return 1;
    }
    method_name = LT_Closure_name(LT_Closure_from_value(method));
    if (expect(LT_Pair_p(method_name), "define-method assigns structured closure name")){
        return 1;
    }
    cursor = method_name;
    if (expect(
        LT_Symbol_p(LT_car(cursor))
            && strcmp(
                LT_Symbol_name(LT_Symbol_from_value(LT_car(cursor))),
                "Pair"
            ) == 0,
        "define-method closure name starts with class"
    )){
        return 1;
    }
    cursor = LT_cdr(cursor);
    if (expect(
        LT_Pair_p(cursor)
            && LT_Symbol_p(LT_car(cursor))
            && strcmp(
                LT_Symbol_name(LT_Symbol_from_value(LT_car(cursor))),
                ">>"
            ) == 0,
        "define-method closure name includes >> separator"
    )){
        return 1;
    }
    cursor = LT_cdr(cursor);
    if (expect(
        LT_Pair_p(cursor)
            && LT_Symbol_p(LT_car(cursor))
            && strcmp(
                LT_Symbol_name(LT_Symbol_from_value(LT_car(cursor))),
                "first"
            ) == 0
            && LT_cdr(cursor) == LT_NIL,
        "define-method closure name ends with selector"
    )){
        return 1;
    }

    (void)LT_eval(
        read_one("(define-method [Pair setFirst: value] (set! (%self-slot car) value) (%self-slot car))"),
        env,
        NULL
    );
    result = LT_eval(read_one("['(9 . 8) setFirst: 42]"), env, NULL);
    return expect(
        LT_Value_is_fixnum(result) && LT_SmallInteger_value(result) == 42,
        "define-method method body can use %self-slot read and write"
    );
}

static int test_define_class_and_method_rectangle_example(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value result;

    result = LT_eval_sequence_string(
        "(define-class Rectangle (Object) (width height))\n"
        "\n"
        "(define-method [[Rectangle class] width: width height: height]\n"
        "    (let ((inst [self alloc]))\n"
        "        (slot-set! inst 'width width)\n"
        "        (slot-set! inst 'height height)\n"
        "        inst))\n"
        "\n"
        "(define-method [Rectangle area]\n"
        "    (* .width .height))\n"
        "\n"
        "[[Rectangle width: 4 height: 5] area]\n",
        env
    );

    return expect(
        LT_Value_is_fixnum(result) && LT_SmallInteger_value(result) == 20,
        "rectangle define-class/define-method example evaluates to area 20"
    );
}

static int test_send_macro_rewrites_literal_super_to_super_send(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value result;

    result = LT_eval_sequence_string(
        "(define-class Parent (Object) ())\n"
        "(define-class Child (Parent) ())\n"
        "(define-method [Parent answer] 11)\n"
        "(define-method [Child answer] [ListTalk:super answer])\n"
        "[(make-instance Child) answer]\n",
        env
    );

    return expect(
        LT_Value_is_fixnum(result) && LT_SmallInteger_value(result) == 11,
        "send macro rewrites literal super receiver to super send"
    );
}

static int test_define_package_special_form(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value value = LT_NIL;

    LT_WITH_PACKAGE(LT_PACKAGE_LISTTALK, {
        value = LT_eval(read_one("(define-package \"TestPackage\")"), env, NULL);
    });

    if (expect(LT_Package_p(value), "define-package returns package object")){
        return 1;
    }
    return expect(
        strcmp(LT_Package_name(LT_Package_from_value(value)), "TestPackage") == 0,
        "define-package creates package by symbol designator"
    );
}

static int test_in_package_special_form(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value result = LT_NIL;
    LT_Package* package = LT_Package_new("EvalInPackage");
    int failed = 0;

    LT_WITH_PACKAGE(LT_PACKAGE_LISTTALK, {
        (void)LT_eval(read_one("(define-package \"EvalInPackage\")"), env, NULL);
        result = LT_eval(read_one("(in-package \"EvalInPackage\")"), env, NULL);
        if (expect(
            LT_Package_p(result)
                && LT_Package_from_value(result) == package,
            "in-package returns selected package"
        )){
            failed = 1;
        }
        if (expect(
            LT_Symbol_package(LT_Symbol_from_value(LT_Symbol_new("x"))) == package,
            "in-package updates current package for subsequent interning"
        )){
            failed = 1;
        }
    });

    return failed;
}

static int test_use_package_special_form(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value value = LT_NIL;
    int failed = 0;

    LT_WITH_PACKAGE(LT_PACKAGE_LISTTALK, {
        (void)LT_eval_sequence_string(
            "(define-package \"PackageSource\") "
            "(in-package \"PackageSource\") "
            "(define exported-value 42) "
            "(in-package \"ListTalk\") "
            "(define-package \"PackageUser\") "
            "(in-package \"PackageUser\") "
            "(use-package \"PackageSource\") "
            "exported-value",
            env
        );
        value = LT_eval(read_one("exported-value"), env, NULL);
        if (expect(
            LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 42,
            "use-package makes used package symbols visible"
        )){
            failed = 1;
        }
    });

    return failed;
}

static int test_use_package_with_nickname(void){
    LT_Environment* env = NULL;
    char source_name[64];
    char user_name[64];
    char nickname[32];
    char source_buffer[480];
    static unsigned int package_counter = 0;
    unsigned int suffix = ++package_counter;
    LT_Package* source_package;
    LT_Package* user_package;
    LT_Package* resolved_package;
    int failed = 0;

    snprintf(source_name, sizeof(source_name), "NickSource%u", suffix);
    snprintf(user_name, sizeof(user_name), "NickUser%u", suffix);
    snprintf(nickname, sizeof(nickname), "ns%u", suffix);
    snprintf(
        source_buffer,
        sizeof(source_buffer),
        "(define-package \"%s\") "
        "(in-package \"%s\") "
        "(define value 99) "
        "(in-package \"ListTalk\") "
        "(define-package \"%s\") "
        "(in-package \"%s\") "
        "(use-package \"%s\" \"%s\")",
        source_name,
        source_name,
        user_name,
        user_name,
        source_name,
        nickname
    );

    LT_WITH_PACKAGE(LT_PACKAGE_LISTTALK, {
        env = LT_new_base_environment();
        (void)LT_eval_sequence_string(source_buffer, env);
        source_package = LT_Package_find(source_name);
        user_package = LT_Package_find(user_name);
        resolved_package = LT_Package_resolve_used_package(user_package, nickname);
        if (expect(source_package != NULL, "source package exists")){
            failed = 1;
        }
        if (expect(user_package != NULL, "user package exists")){
            failed = 1;
        }
        if (expect(
            resolved_package == source_package,
            "use-package nickname resolves package-prefixed symbols"
        )){
            failed = 1;
        }
    });

    return failed;
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

static int test_compiler_fold_non_constant_symbol_is_unchanged(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value symbol_x = LT_Symbol_new("x");
    LT_Value folded;

    LT_Environment_bind(env, symbol_x, LT_SmallInteger_new(7), 0);
    folded = LT_compiler_fold_expression(symbol_x, env);
    return expect(
        folded == symbol_x,
        "compiler fold keeps non-constant symbol unchanged"
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

    if (expect(LT_ImmutableList_p(folded), "compiler fold returns immutable folded application")){
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
        LT_Symbol_p(LT_car(LT_cdr(args)))
            && strcmp(LT_Symbol_name(LT_Symbol_from_value(LT_car(LT_cdr(args)))), "y") == 0,
        "unresolved argument is kept as expression"
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

    if (expect(LT_ImmutableList_p(folded), "special form fold returns immutable list form")){
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
        LT_Symbol_p(LT_car(LT_cdr(LT_cdr(arguments))))
            && strcmp(
                LT_Symbol_name(
                    LT_Symbol_from_value(LT_car(LT_cdr(LT_cdr(arguments))))
                ),
                "y"
            ) == 0,
        "special form expression keeps unresolved symbol as expression"
    );
}

static int test_compiler_fold_constant_pair_is_quoted_expression(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value pair_constant = read_one("(1 2)");
    LT_Value folded;
    LT_Value arguments;

    LT_Environment_bind(
        env,
        LT_Symbol_new("x"),
        pair_constant,
        LT_ENV_BINDING_FLAG_CONSTANT
    );
    folded = LT_compiler_fold_expression(read_one("x"), env);

    if (expect(LT_ImmutableList_p(folded), "constant pair fold returns immutable expression form")){
        return 1;
    }
    if (expect(
        LT_SpecialForm_p(LT_car(folded)),
        "constant pair fold wraps pair in quote expression"
    )){
        return 1;
    }
    arguments = LT_cdr(folded);
    return expect(
        LT_ImmutableList_p(arguments)
            && LT_car(arguments) == pair_constant
            && LT_cdr(arguments) == LT_NIL,
        "constant pair fold keeps quoted payload"
    );
}

static int test_compiler_expression_constant_value_from_quote_expression(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value folded = LT_compiler_fold_expression(read_one("'(1 2)"), env);
    LT_Value constant_value = LT_compiler_expression_constant_value(folded, env);

    if (expect(LT_Pair_p(constant_value), "constant value recognizes quoted pair")){
        return 1;
    }
    return expect(
        LT_Value_is_fixnum(LT_car(constant_value))
            && LT_SmallInteger_value(LT_car(constant_value)) == 1,
        "constant value returns quoted payload"
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
        LT_ImmutableList_p(folded),
        "compiler fold keeps impure primitive as immutable application"
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
    if (expect(LT_ImmutableList_p(folded), "fold-expression primitive returns immutable form")){
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
        LT_Symbol_p(LT_car(LT_cdr(arguments)))
            && strcmp(LT_Symbol_name(LT_Symbol_from_value(LT_car(LT_cdr(arguments)))), "y") == 0,
        "fold-expression primitive keeps unresolved symbol"
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
    if (expect(LT_ImmutableList_p(folded), "captured environment can be reused")){
        return 1;
    }
    arguments = LT_cdr(folded);
    return expect(
        LT_Value_is_fixnum(LT_car(arguments))
            && LT_SmallInteger_value(LT_car(arguments)) == 7,
        "get-current-environment captures lexical environment"
    );
}

static int test_load_bang_loads_first_matching_module(void){
    char directory[512];
    char module_path[512];
    char source[1024];
    FILE* file;
    LT_Environment* env;
    LT_Value value;
    int failed = 0;

    snprintf(
        directory,
        sizeof(directory),
        "/tmp/listtalk-load-test-%ld",
        (long)getpid()
    );
    if (mkdir(directory, 0700) != 0){
        return fail("unable to create module fixture directory");
    }

    snprintf(module_path, sizeof(module_path), "%s/foo.lt", directory);
    file = fopen(module_path, "w");
    if (file == NULL){
        rmdir(directory);
        return fail("unable to create module fixture");
    }
    fputs("(provide :foo)\n(define loaded-from-module 42)\n", file);
    fclose(file);

    snprintf(
        source,
        sizeof(source),
        "(load! :foo \"%s\")\nloaded-from-module",
        directory
    );

    env = LT_new_base_environment();
    LT_WITH_PACKAGE(LT_PACKAGE_LISTTALK_USER, {
        value = LT_eval_sequence_string(source, env);
    });
    failed |= expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 42,
        "load! loads module file into target environment"
    );

    value = LT_eval_sequence_string("(memq :foo ListTalk:%modules)", env);
    failed |= expect(
        LT_Pair_p(value)
            && LT_car(value) == LT_Symbol_new_in(LT_PACKAGE_KEYWORD, "foo"),
        "provide records loaded module"
    );

    remove(module_path);
    rmdir(directory);
    return failed;
}

static int test_load_bang_call_resolvers_do_not_mutate_global_resolvers(void){
    char directory[512];
    char module_path[512];
    char source[1024];
    FILE* file;
    LT_Environment* env;
    LT_Value value;
    int failed = 0;

    snprintf(
        directory,
        sizeof(directory),
        "/tmp/listtalk-load-local-resolver-test-%ld",
        (long)getpid()
    );
    if (mkdir(directory, 0700) != 0){
        return fail("unable to create local resolver fixture directory");
    }

    snprintf(module_path, sizeof(module_path), "%s/bar.lt", directory);
    file = fopen(module_path, "w");
    if (file == NULL){
        rmdir(directory);
        return fail("unable to create local resolver module fixture");
    }
    fputs("(provide :bar)\n(define loaded-from-local-resolver 91)\n", file);
    fclose(file);

    snprintf(
        source,
        sizeof(source),
        "(define saved-resolvers ListTalk:module-resolvers)\n"
        "(load! :bar \"%s\")\n"
        "(list loaded-from-local-resolver "
        "      (eq? saved-resolvers ListTalk:module-resolvers))",
        directory
    );

    env = LT_new_base_environment();
    LT_WITH_PACKAGE(LT_PACKAGE_LISTTALK_USER, {
        value = LT_eval_sequence_string(source, env);
    });
    failed |= expect(
        LT_Pair_p(value)
            && LT_Value_is_fixnum(LT_car(value))
            && LT_SmallInteger_value(LT_car(value)) == 91,
        "load! uses call-local resolvers"
    );
    failed |= expect(
        LT_Pair_p(LT_cdr(value))
            && LT_Value_is_boolean(LT_car(LT_cdr(value)))
            && LT_Value_boolean_value(LT_car(LT_cdr(value))),
        "load! call-local resolvers do not mutate module-resolvers"
    );

    remove(module_path);
    rmdir(directory);
    return failed;
}

static int test_load_bang_loads_native_module(void){
    const char* resolver = os_module_resolver_path();
    char source[512];
    LT_Environment* env;
    LT_Value value;
    LT_Value provided;
    int failed = 0;

    if (resolver == NULL){
        return fail("unable to locate os.ltm fixture");
    }

    snprintf(
        source,
        sizeof(source),
        "(load! :os \"%s\")\n"
        "(primitive? ListTalk-OS:exit)",
        resolver
    );

    env = LT_new_base_environment();
    value = LT_eval_sequence_string(source, env);
    failed |= expect(
        LT_Value_is_boolean(value) && LT_Value_boolean_value(value),
        "load! loads native module primitive"
    );

    provided = LT_eval_sequence_string("(memq :os ListTalk:%modules)", env);
    failed |= expect(
        LT_Pair_p(provided)
            && LT_car(provided) == LT_Symbol_new_in(LT_PACKAGE_KEYWORD, "os"),
        "native module records provided module"
    );

    return failed;
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

    if (expect(LT_Error_p(value), "handler-bind catches error condition")){
        return 1;
    }
    return expect(
        strcmp(condition_message_cstr(value), "boom") == 0,
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

static int test_symbol_package_slot_is_readonly(void){
    LT_Value slot_value = eval_one("(slot-ref 'alpha 'package)");
    LT_Value readonly_error = eval_one(
        "(catch :t "
        "  (handler-bind (lambda (c) (throw :t c)) "
        "    (slot-set! 'alpha 'package 1)))"
    );

    if (expect(
        LT_Package_p(slot_value)
            && LT_Package_from_value(slot_value) == LT_PACKAGE_LISTTALK,
        "symbol package slot exposes package object"
    )){
        return 1;
    }
    if (expect(
        LT_Error_p(readonly_error),
        "setting readonly symbol package slot signals error condition"
    )){
        return 1;
    }
    return expect(
        strcmp(condition_message_cstr(readonly_error), "Readonly slot") == 0,
        "symbol package slot is readonly"
    );
}

static int test_precedence_list_initialized(void){
    LT_Value precedence_list = LT_Class_precedence_list(&LT_SmallInteger_class);

    if (expect(precedence_list != LT_NIL, "class precedence list exists")){
        return 1;
    }
    if (expect(
        LT_ImmutableList_car(precedence_list) == LT_STATIC_CLASS(LT_SmallInteger),
        "precedence list starts with class itself"
    )){
        return 1;
    }
    precedence_list = LT_ImmutableList_cdr(precedence_list);
    if (expect(
        LT_ImmutableList_car(precedence_list) == LT_STATIC_CLASS(LT_Integer),
        "precedence list contains Integer"
    )){
        return 1;
    }
    precedence_list = LT_ImmutableList_cdr(precedence_list);
    if (expect(
        LT_ImmutableList_car(precedence_list) == LT_STATIC_CLASS(LT_RationalNumber),
        "precedence list contains RationalNumber"
    )){
        return 1;
    }
    precedence_list = LT_ImmutableList_cdr(precedence_list);
    if (expect(
        LT_ImmutableList_car(precedence_list) == LT_STATIC_CLASS(LT_RealNumber),
        "precedence list contains RealNumber"
    )){
        return 1;
    }
    precedence_list = LT_ImmutableList_cdr(precedence_list);
    if (expect(
        LT_ImmutableList_car(precedence_list) == LT_STATIC_CLASS(LT_ComplexNumber),
        "precedence list contains ComplexNumber"
    )){
        return 1;
    }
    precedence_list = LT_ImmutableList_cdr(precedence_list);
    if (expect(
        LT_ImmutableList_car(precedence_list) == LT_STATIC_CLASS(LT_Number),
        "precedence list contains Number"
    )){
        return 1;
    }
    precedence_list = LT_ImmutableList_cdr(precedence_list);
    if (expect(
        LT_ImmutableList_car(precedence_list) == LT_STATIC_CLASS(LT_Object),
        "precedence list contains root object class"
    )){
        return 1;
    }
    precedence_list = LT_ImmutableList_cdr(precedence_list);
    return expect(
        precedence_list == LT_NIL,
        "precedence list is nil terminated"
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
        LT_Value_is_instance_of(one, LT_STATIC_CLASS(LT_Integer)),
        "fixnum is instance of Integer"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_instance_of(one, LT_STATIC_CLASS(LT_RationalNumber)),
        "fixnum is instance of RationalNumber"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_instance_of(one, LT_STATIC_CLASS(LT_RealNumber)),
        "fixnum is instance of RealNumber"
    )){
        return 1;
    }
    if (expect(
        LT_Value_is_instance_of(one, LT_STATIC_CLASS(LT_ComplexNumber)),
        "fixnum is instance of ComplexNumber"
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

static int test_numeric_abstract_classes_are_bound(void){
    LT_Value complex_class = eval_one("ComplexNumber");
    LT_Value integer_class = eval_one("Integer");
    LT_Value rational_class = eval_one("RationalNumber");
    LT_Value real_class = eval_one("RealNumber");

    if (expect(
        (LT_Class*)LT_VALUE_POINTER_VALUE(complex_class) == &LT_ComplexNumber_class,
        "ComplexNumber class is bound"
    )){
        return 1;
    }
    if (expect(
        (LT_Class*)LT_VALUE_POINTER_VALUE(integer_class) == &LT_Integer_class,
        "Integer class is bound"
    )){
        return 1;
    }
    if (expect(
        (LT_Class*)LT_VALUE_POINTER_VALUE(rational_class) == &LT_RationalNumber_class,
        "RationalNumber class is bound"
    )){
        return 1;
    }
    return expect(
        (LT_Class*)LT_VALUE_POINTER_VALUE(real_class) == &LT_RealNumber_class,
        "RealNumber class is bound"
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
#define RUN_TEST(TEST_FN) \
    do { \
        LT_set_current_package(LT_PACKAGE_LISTTALK); \
        failures += (TEST_FN)(); \
    } while (0)

    RUN_TEST(test_add);
    RUN_TEST(test_subtract_unary);
    RUN_TEST(test_multiply);
    RUN_TEST(test_divide);
    RUN_TEST(test_add_float_mixed);
    RUN_TEST(test_divide_float_mixed);
    RUN_TEST(test_subtract_unary_float);
    RUN_TEST(test_integer_divide_still_fixnum);
    RUN_TEST(test_fixnum_overflow_promotes_to_big_integer);
    RUN_TEST(test_fraction_addition_is_reduced);
    RUN_TEST(test_big_integer_multiplication_beyond_int128);
    RUN_TEST(test_complex_addition);
    RUN_TEST(test_complex_multiplication);
    RUN_TEST(test_complex_division);
    RUN_TEST(test_real_complex_mixed_addition);
    RUN_TEST(test_fraction_multiplication_canonicalizes_to_integer);
    RUN_TEST(test_symbol_lookup);
    RUN_TEST(test_display_primitive_returns_argument);
    RUN_TEST(test_keyword_self_evaluating_when_unbound);
    RUN_TEST(test_type_of_primitive);
    RUN_TEST(test_native_class_lookup);
    RUN_TEST(test_class_slots_primitive);
    RUN_TEST(test_send_primitive_uses_direct_method_dictionary);
    RUN_TEST(test_send_primitive_uses_precedence_lookup_and_cache);
    RUN_TEST(test_environment_invocation_context_lookup_walks_parent_frames);
    RUN_TEST(test_send_passes_invocation_context_kind_to_primitive_method);
    RUN_TEST(test_send_passes_next_precedence_tail_as_invocation_context_data);
    RUN_TEST(test_super_send_c_api_uses_explicit_precedence_list);
    RUN_TEST(test_basic_object_and_class_methods);
    RUN_TEST(test_basic_pair_methods);
    RUN_TEST(test_basic_string_and_vector_methods);
    RUN_TEST(test_make_class_primitive);
    RUN_TEST(test_make_instance_primitive);
    RUN_TEST(test_class_alloc_method);
    RUN_TEST(test_make_instance_non_allocatable_class_errors);
    RUN_TEST(test_class_alloc_non_allocatable_class_errors);
    RUN_TEST(test_class_add_method_with_selector_method);
    RUN_TEST(test_class_add_method_invalidates_method_cache);
    RUN_TEST(test_cons_primitive);
    RUN_TEST(test_car_primitive);
    RUN_TEST(test_cdr_primitive);
    RUN_TEST(test_pair_predicate_primitive);
    RUN_TEST(test_numeric_equal_primitive);
    RUN_TEST(test_numeric_equal_type_error_on_non_number);
    RUN_TEST(test_eq_primitive);
    RUN_TEST(test_eqv_primitive);
    RUN_TEST(test_equal_primitive);
    RUN_TEST(test_not_primitive);
    RUN_TEST(test_core_type_predicates);
    RUN_TEST(test_list_constructor_primitive);
    RUN_TEST(test_list_predicate_primitive);
    RUN_TEST(test_immutable_list_interops_with_pairs);
    RUN_TEST(test_immutable_list_methods);
    RUN_TEST(test_immutable_list_from_list);
    RUN_TEST(test_append_primitive);
    RUN_TEST(test_memq_primitive);
    RUN_TEST(test_assoc_primitives);
    RUN_TEST(test_cxxxr_primitives);
    RUN_TEST(test_string_predicate_primitive);
    RUN_TEST(test_string_length_primitive);
    RUN_TEST(test_string_ref_primitive);
    RUN_TEST(test_character_predicate_primitive);
    RUN_TEST(test_string_to_character_list_primitive);
    RUN_TEST(test_character_list_to_string_primitive);
    RUN_TEST(test_string_append_primitive);
    RUN_TEST(test_vector_predicate_primitive);
    RUN_TEST(test_vector_length_primitive);
    RUN_TEST(test_vector_constructor_and_ref_primitive);
    RUN_TEST(test_make_vector_primitive);
    RUN_TEST(test_vector_set_bang_primitive);
    RUN_TEST(test_slot_ref_primitive);
    RUN_TEST(test_slot_set_bang_primitive);
    RUN_TEST(test_slot_table_includes_superclass_slots);
    RUN_TEST(test_metaclass_has_valid_name_slot);
    RUN_TEST(test_quote);
    RUN_TEST(test_quote_reader_syntax);
    RUN_TEST(test_quasiquote_unquote);
    RUN_TEST(test_quasiquote_reader_syntax);
    RUN_TEST(test_quasiquote_unquote_splicing);
    RUN_TEST(test_lambda_application);
    RUN_TEST(test_lambda_rest_parameter_dotted);
    RUN_TEST(test_lambda_rest_parameter_symbol);
    RUN_TEST(test_tail_call_optimization_deep_recursion);
    RUN_TEST(test_if_special_form);
    RUN_TEST(test_and_special_form);
    RUN_TEST(test_or_special_form);
    RUN_TEST(test_cond_special_form);
    RUN_TEST(test_begin_special_form);
    RUN_TEST(test_let_special_form);
    RUN_TEST(test_define_special_form);
    RUN_TEST(test_define_function_shorthand);
    RUN_TEST(test_define_function_shorthand_is_constant);
    RUN_TEST(test_define_macro_shorthand);
    RUN_TEST(test_define_macro_shorthand_is_constant);
    RUN_TEST(test_lambda_macro_expands_to_named_nil_closure);
    RUN_TEST(test_define_function_shorthand_sets_closure_name);
    RUN_TEST(test_closure_debug_print_includes_name);
    RUN_TEST(test_anonymous_closure_debug_print_includes_address);
    RUN_TEST(test_symbol_uninterned_and_gensym_c_api);
    RUN_TEST(test_gensym_primitive);
    RUN_TEST(test_symbol_class_methods_for_uninterned_and_gensym);
    RUN_TEST(test_symbol_name_method);
    RUN_TEST(test_with_gensyms_macro);
    RUN_TEST(test_set_bang_special_form);
    RUN_TEST(test_set_bang_parent_binding);
    RUN_TEST(test_set_bang_macro_dispatches_self_slot);
    RUN_TEST(test_define_class_macro);
    RUN_TEST(test_define_method_macro);
    RUN_TEST(test_define_class_and_method_rectangle_example);
    RUN_TEST(test_send_macro_rewrites_literal_super_to_super_send);
    RUN_TEST(test_define_package_special_form);
    RUN_TEST(test_in_package_special_form);
    RUN_TEST(test_use_package_special_form);
    RUN_TEST(test_use_package_with_nickname);
    RUN_TEST(test_macro_special_form_constructs_macro);
    RUN_TEST(test_macro_expansion_is_evaluated);
    RUN_TEST(test_macro_expansion_uses_call_environment);
    RUN_TEST(test_compiler_fold_non_constant_symbol_is_unchanged);
    RUN_TEST(test_compiler_fold_application_folds_operator_and_arguments);
    RUN_TEST(test_compiler_fold_special_form_uses_special_form_reference);
    RUN_TEST(test_compiler_fold_constant_pair_is_quoted_expression);
    RUN_TEST(test_compiler_expression_constant_value_from_quote_expression);
    RUN_TEST(test_compiler_fold_expands_macros);
    RUN_TEST(test_compiler_fold_pure_primitive_constant_folds);
    RUN_TEST(test_compiler_fold_impure_primitive_is_not_constant_folded);
    RUN_TEST(test_macroexpand_special_form);
    RUN_TEST(test_fold_expression_special_form);
    RUN_TEST(test_get_current_environment_special_form);
    RUN_TEST(test_load_bang_loads_first_matching_module);
    RUN_TEST(test_load_bang_call_resolvers_do_not_mutate_global_resolvers);
    RUN_TEST(test_load_bang_loads_native_module);
    RUN_TEST(test_catch_returns_body_value_without_throw);
    RUN_TEST(test_throw_is_caught_by_matching_tag);
    RUN_TEST(test_throw_skips_to_outer_matching_catch);
    RUN_TEST(test_unwind_protect_runs_cleanup_on_normal_path);
    RUN_TEST(test_unwind_protect_runs_cleanup_on_throw_path);
    RUN_TEST(test_handler_bind_special_form_binds_handler_for_body);
    RUN_TEST(test_symbol_package_slot_is_readonly);
    RUN_TEST(test_symbol_class_inherits_object);
    RUN_TEST(test_precedence_list_initialized);
    RUN_TEST(test_value_is_instance_of_uses_precedence_list);
    RUN_TEST(test_numeric_abstract_classes_are_bound);
    RUN_TEST(test_boolean_constants);
    RUN_TEST(test_character_api_uses_unicode_codepoints);

#undef RUN_TEST

    if (failures == 0){
        puts("eval tests passed");
        return 0;
    }

    fprintf(stderr, "%d eval test(s) failed\n", failures);
    return 1;
}
