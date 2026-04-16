/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Package.h>
#include <ListTalk/classes/Reader.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/macros/arg_macros.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

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

static int expect_near_double(double actual,
                              double expected,
                              double epsilon,
                              const char* message){
    return expect(fabs(actual - expected) <= epsilon, message);
}

static LT_Value read_one(const char* source){
    LT_Reader* reader = LT_Reader_new(LT_NIL);
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

LT_DEFINE_PRIMITIVE(
    primitive_test_increment,
    "test-increment",
    "(x)",
    "Test helper primitive: increment fixnum."
){
    LT_Value cursor = arguments;
    LT_Value x;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, x);
    LT_ARG_END(cursor);
    return LT_Number_add2(x, LT_SmallInteger_new(1));
}

LT_DEFINE_PRIMITIVE(
    primitive_test_add_two,
    "test-add-two",
    "(x y)",
    "Test helper primitive: add two numbers."
){
    LT_Value cursor = arguments;
    LT_Value x;
    LT_Value y;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, x);
    LT_OBJECT_ARG(cursor, y);
    LT_ARG_END(cursor);
    return LT_Number_add2(x, y);
}

static int primitive_test_for_each_count = 0;

LT_DEFINE_PRIMITIVE(
    primitive_test_count_for_each,
    "test-count-for-each",
    "(x)",
    "Test helper primitive: count one for-each call."
){
    LT_Value cursor = arguments;
    LT_Value x;
    (void)x;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, x);
    LT_ARG_END(cursor);
    primitive_test_for_each_count++;
    return LT_NIL;
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

static int test_send_primitive_uses_direct_method_dictionary(void){
    LT_Value selector = LT_Symbol_new_in(LT_PACKAGE_KEYWORD, "sum");
    LT_Value result;

    LT_Class_addMethod(
        &LT_Pair_class,
        selector,
        LT_Primitive_from_static(&primitive_test_pair_sum_method)
    );

    result = eval_one("[(cons 1 2) sum]");
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

    result = eval_one("[(cons 1 2) class-name]");
    if (expect(
        LT_Symbol_p(result)
            && strcmp(LT_Symbol_name(LT_Symbol_from_value(result)), "MutablePair") == 0,
        "send resolves method on superclass precedence list"
    )){
        return 1;
    }

    pair_cache = LT_IdentityDictionary_from_value(LT_MutablePair_class.method_cache);
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

static int test_class_add_method_invalidates_method_cache(void){
    LT_Value selector = LT_Symbol_new_in(LT_PACKAGE_KEYWORD, "class-name");
    LT_Value result;

    LT_Class_addMethod(
        &LT_Object_class,
        selector,
        LT_Primitive_from_static(&primitive_test_object_class_name_method)
    );
    (void)eval_one("[(cons 1 2) class-name]");

    LT_Class_addMethod(
        &LT_Pair_class,
        selector,
        LT_Primitive_from_static(&primitive_test_pair_override_method)
    );
    result = eval_one("[(cons 1 2) class-name]");

    return expect(
        LT_Symbol_p(result)
            && strcmp(LT_Symbol_name(LT_Symbol_from_value(result)), "PairOverride") == 0,
        "LT_Class_addMethod invalidates stale method cache entries"
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

static int test_source_location_class(void){
    LT_Value value = LT_SourceLocation_new(12u, 34u);

    if (expect(LT_SourceLocation_p(value), "LT_SourceLocation_new returns SourceLocation")){
        return 1;
    }
    if (expect(
        LT_SourceLocation_line(value) == 12u,
        "LT_SourceLocation_line returns stored line"
    )){
        return 1;
    }
    if (expect(
        LT_SourceLocation_column(value) == 34u,
        "LT_SourceLocation_column returns stored column"
    )){
        return 1;
    }
    return expect(
        LT_Value_class(value) == &LT_SourceLocation_class,
        "SourceLocation immediate maps to SourceLocation class"
    );
}

static int test_immutable_list_trailer_values(void){
    LT_Value values[] = {
        LT_SmallInteger_new(20),
        LT_SmallInteger_new(21),
    };
    LT_Value source_location = LT_SourceLocation_new(7u, 9u);
    LT_Value source_file = (LT_Value)(uintptr_t)LT_String_new_cstr("runtime/test.lt");
    LT_Value original_expression = LT_cons(
        LT_Symbol_new("quote"),
        LT_cons(LT_SmallInteger_new(99), LT_NIL)
    );
    LT_Value immutable_list = LT_ImmutableList_new_with_trailer(
        2,
        values,
        LT_SmallInteger_new(22),
        source_location,
        source_file,
        original_expression
    );
    if (expect(
        LT_cdr(LT_cdr(immutable_list)) == LT_SmallInteger_new(22),
        "immutable list trailer preserves dotted tail"
    )){
        return 1;
    }
    if (expect(
        LT_ImmutableList_source_location(immutable_list) == source_location,
        "immutable list exposes source location trailer"
    )){
        return 1;
    }
    if (expect(
        LT_SourceLocation_line(LT_ImmutableList_source_location(immutable_list)) == 7u,
        "source location trailer keeps line"
    )){
        return 1;
    }
    if (expect(
        LT_ImmutableList_source_file(immutable_list) == source_file,
        "immutable list exposes source file trailer"
    )){
        return 1;
    }
    if (expect(
        strcmp(
            LT_String_value_cstr(
                LT_String_from_value(LT_ImmutableList_source_file(immutable_list))
            ),
            "runtime/test.lt"
        ) == 0,
        "source file trailer keeps filename"
    )){
        return 1;
    }
    return expect(
        LT_ImmutableList_original_expression(immutable_list) == original_expression,
        "immutable list exposes original expression trailer"
    );
}

static int test_immutable_list_missing_trailer_values_are_nil(void){
    LT_Value values[] = {
        LT_SmallInteger_new(30),
    };
    LT_Value immutable_list = LT_ImmutableList_new(1, values);

    if (expect(
        LT_ImmutableList_source_location(immutable_list) == LT_NIL,
        "missing source location trailer is nil"
    )){
        return 1;
    }
    if (expect(
        LT_ImmutableList_source_file(immutable_list) == LT_NIL,
        "missing source file trailer is nil"
    )){
        return 1;
    }
    return expect(
        LT_ImmutableList_original_expression(immutable_list) == LT_NIL,
        "missing original expression trailer is nil"
    );
}

static int test_list_map_single_c_api(void){
    LT_Value list = LT_cons(
        LT_SmallInteger_new(1),
        LT_cons(LT_SmallInteger_new(2), LT_cons(LT_SmallInteger_new(3), LT_NIL))
    );
    LT_Value mapped = LT_List_map(
        LT_Primitive_from_static(&primitive_test_increment),
        list
    );

    if (expect(LT_Value_equal_p(mapped, eval_one("'(2 3 4)")), "LT_List_map maps one list")){
        return 1;
    }
    return expect(
        LT_Value_equal_p(list, eval_one("'(1 2 3)")),
        "LT_List_map leaves source list contents unchanged"
    );
}

static int test_list_at_c_api(void){
    LT_Value list = eval_one("'(1 2 3)");
    LT_Value value = LT_List_at(list, 1);

    return expect(
        LT_Value_is_fixnum(value) && LT_SmallInteger_value(value) == 2,
        "LT_List_at returns indexed item"
    );
}

static int test_list_map_many_c_api(void){
    LT_Value lists[] = {
        LT_cons(
            LT_SmallInteger_new(1),
            LT_cons(LT_SmallInteger_new(2), LT_cons(LT_SmallInteger_new(3), LT_NIL))
        ),
        LT_cons(
            LT_SmallInteger_new(10),
            LT_cons(
                LT_SmallInteger_new(20),
                LT_cons(LT_SmallInteger_new(30), LT_cons(LT_SmallInteger_new(40), LT_NIL))
            )
        ),
    };
    LT_Value mapped = LT_List_map_many(
        LT_Primitive_from_static(&primitive_test_add_two),
        2,
        lists
    );

    return expect(
        LT_Value_equal_p(mapped, eval_one("'(11 22 33)")),
        "LT_List_map_many terminates on shortest list"
    );
}

static int test_list_for_each_c_api(void){
    LT_Value list = eval_one("'(1 2 3)");

    primitive_test_for_each_count = 0;
    LT_List_for_each(
        LT_Primitive_from_static(&primitive_test_count_for_each),
        list
    );

    return expect(
        primitive_test_for_each_count == 3,
        "LT_List_for_each applies callable across list"
    );
}

static int test_list_any_every_c_api(void){
    LT_Value list = eval_one("'(1 2 3)");
    LT_Value matches_two = eval_one("(lambda (x) (= x 2))");
    LT_Value below_four = eval_one("(lambda (x) (< x 4))");
    LT_Value below_three = eval_one("(lambda (x) (< x 3))");

    if (expect(
        LT_List_any(matches_two, list) == LT_TRUE,
        "LT_List_any returns true for matching element"
    )){
        return 1;
    }
    if (expect(
        LT_List_every(below_four, list) == LT_TRUE,
        "LT_List_every returns true when all elements match"
    )){
        return 1;
    }
    return expect(
        LT_List_every(below_three, list) == LT_FALSE,
        "LT_List_every returns false when an element misses"
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
    result = LT_eval(read_one("[(cons 9 8) first]"), env, NULL);
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
    result = LT_eval(read_one("[(cons 9 8) setFirst: 42]"), env, NULL);
    return expect(
        LT_Value_is_fixnum(result) && LT_SmallInteger_value(result) == 42,
        "define-method method body can use %self-slot read and write"
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
    LT_Value expression = read_one("(+ x y)");
    LT_Value folded;
    LT_Value args;

    LT_Environment_bind(
        env,
        LT_Symbol_new("x"),
        LT_SmallInteger_new(5),
        LT_ENV_BINDING_FLAG_CONSTANT
    );
    folded = LT_compiler_fold_expression(expression, env);

    if (expect(LT_ImmutableList_p(folded), "compiler fold returns immutable folded application")){
        return 1;
    }
    if (expect(LT_Primitive_p(LT_car(folded)), "folded operator becomes value")){
        return 1;
    }
    if (expect(
        LT_ImmutableList_original_expression(folded) == expression,
        "folded application keeps original expression trailer"
    )){
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
    LT_Value expression = read_one("(id-macro (+ 1 2))");
    LT_Value folded;
    LT_Value macro_value;

    macro_value = LT_eval(read_one("(macro (lambda (x) x))"), env, NULL);
    LT_Environment_bind(
        env,
        LT_Symbol_new("id-macro"),
        macro_value,
        LT_ENV_BINDING_FLAG_CONSTANT
    );

    folded = LT_compiler_fold_expression(expression, env);

    if (expect(
        LT_Value_is_fixnum(folded) && LT_SmallInteger_value(folded) == 3,
        "macro expansion result is folded through pure primitive"
    )){
        return 1;
    }

    folded = LT_compiler_macroexpand(expression, env);
    if (expect(
        LT_ImmutableList_p(folded),
        "compiler macroexpand normalizes expanded form to immutable list"
    )){
        return 1;
    }
    return expect(
        LT_ImmutableList_original_expression(folded) == expression,
        "compiler macroexpand keeps original expression trailer"
    );
}

static int test_compiler_macroexpand_preserves_expansion_chain(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value expression = read_one("(outer-macro)");
    LT_Value outer_macro_value;
    LT_Value inner_macro_value;
    LT_Value expanded;
    LT_Value previous_expression;

    outer_macro_value = LT_eval(read_one("(macro (lambda () '(inner-macro)))"), env, NULL);
    inner_macro_value = LT_eval(read_one("(macro (lambda () '(+ 1 2)))"), env, NULL);
    LT_Environment_bind(
        env,
        LT_Symbol_new("outer-macro"),
        outer_macro_value,
        LT_ENV_BINDING_FLAG_CONSTANT
    );
    LT_Environment_bind(
        env,
        LT_Symbol_new("inner-macro"),
        inner_macro_value,
        LT_ENV_BINDING_FLAG_CONSTANT
    );

    expanded = LT_compiler_macroexpand(expression, env);
    if (expect(
        LT_ImmutableList_p(expanded),
        "compiler macroexpand returns immutable final expansion"
    )){
        return 1;
    }

    previous_expression = LT_ImmutableList_original_expression(expanded);
    if (expect(
        LT_ImmutableList_p(previous_expression),
        "final expansion keeps previous expansion in original-expression trailer"
    )){
        return 1;
    }
    if (expect(
        LT_Symbol_p(LT_car(previous_expression))
            && strcmp(
                LT_Symbol_name(LT_Symbol_from_value(LT_car(previous_expression))),
                "inner-macro"
            ) == 0,
        "expansion chain keeps intermediate expansion"
    )){
        return 1;
    }
    return expect(
        LT_ImmutableList_original_expression(previous_expression) == expression,
        "intermediate expansion points back to original expression"
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

static int test_compiler_fold_quasiquote_folds_unquote_expression(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value macro_value;
    LT_Value folded;
    LT_Value template;
    LT_Value element;
    LT_Value unquote_arguments;

    macro_value = LT_eval(read_one("(macro (lambda (x) x))"), env, NULL);
    LT_Environment_bind(
        env,
        LT_Symbol_new("id-macro"),
        macro_value,
        LT_ENV_BINDING_FLAG_CONSTANT
    );

    folded = LT_compiler_fold_expression(read_one("`(,(id-macro (+ 1 2)))"), env);

    if (expect(
        LT_ImmutableList_p(folded),
        "quasiquote fold returns immutable list form"
    )){
        return 1;
    }
    if (expect(
        LT_SpecialForm_p(LT_car(folded)),
        "quasiquote fold resolves quasiquote operator"
    )){
        return 1;
    }

    template = LT_car(LT_cdr(folded));
    if (expect(LT_Pair_p(template), "quasiquote fold keeps list template")){
        return 1;
    }

    element = LT_car(template);
    if (expect(LT_Pair_p(element), "quasiquote fold keeps unquote form")){
        return 1;
    }
    if (expect(
        LT_Symbol_p(LT_car(element))
            && strcmp(LT_Symbol_name(LT_Symbol_from_value(LT_car(element))), "unquote") == 0,
        "quasiquote fold preserves unquote operator"
    )){
        return 1;
    }

    unquote_arguments = LT_cdr(element);
    return expect(
        LT_Value_is_fixnum(LT_car(unquote_arguments))
            && LT_SmallInteger_value(LT_car(unquote_arguments)) == 3,
        "quasiquote fold constant-folds unquote body"
    );
}

static int test_compiler_fold_quasiquote_preserves_nested_unquote_depth(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value macro_value;
    LT_Value folded;
    LT_Value template;
    LT_Value nested_quasiquote;
    LT_Value nested_template;
    LT_Value nested_unquote;
    LT_Value nested_arguments;

    macro_value = LT_eval(read_one("(macro (lambda (x) x))"), env, NULL);
    LT_Environment_bind(
        env,
        LT_Symbol_new("id-macro"),
        macro_value,
        LT_ENV_BINDING_FLAG_CONSTANT
    );

    folded = LT_compiler_fold_expression(
        read_one("`(inner `(,(id-macro (+ 1 2))))"),
        env
    );

    template = LT_car(LT_cdr(folded));
    nested_quasiquote = LT_car(LT_cdr(template));
    if (expect(LT_Pair_p(nested_quasiquote), "nested quasiquote stays in template")){
        return 1;
    }
    if (expect(
        LT_Symbol_p(LT_car(nested_quasiquote))
            && strcmp(LT_Symbol_name(LT_Symbol_from_value(LT_car(nested_quasiquote))), "quasiquote") == 0,
        "nested quasiquote operator is preserved"
    )){
        return 1;
    }

    nested_template = LT_car(LT_cdr(nested_quasiquote));
    nested_unquote = LT_car(nested_template);
    if (expect(LT_Pair_p(nested_unquote), "nested unquote stays quoted by inner quasiquote")){
        return 1;
    }

    nested_arguments = LT_cdr(nested_unquote);
    return expect(
        LT_Pair_p(LT_car(nested_arguments))
            && LT_Symbol_p(LT_car(LT_car(nested_arguments)))
            && strcmp(
                LT_Symbol_name(LT_Symbol_from_value(LT_car(LT_car(nested_arguments)))),
                "id-macro"
            ) == 0,
        "nested quasiquote does not fold escaped unquote body"
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

static int expect_complex_value_near(const char* expression,
                                     double expected_real,
                                     double expected_imaginary,
                                     const char* description){
    LT_Value value = eval_one(expression);
    double actual_real;
    double actual_imaginary;

    if (LT_InexactComplexNumber_p(value)){
        actual_real = LT_InexactComplexNumber_real(value);
        actual_imaginary = LT_InexactComplexNumber_imaginary(value);
    } else if (LT_Float_p(value)){
        actual_real = LT_Float_value(value);
        actual_imaginary = 0.0;
    } else {
        return fail(description);
    }

    if (expect_near_double(actual_real, expected_real, 1e-12, description)){
        return 1;
    }
    return expect_near_double(actual_imaginary, expected_imaginary, 1e-12, description);
}

static int test_complex_transcendentals(void){
    double pi = acos(-1.0);

    if (expect_complex_value_near(
        "(sin 1+1i)",
        sin(1.0) * cosh(1.0),
        cos(1.0) * sinh(1.0),
        "sin computes complex result"
    )){
        return 1;
    }
    if (expect_complex_value_near(
        "(log -1)",
        0.0,
        pi,
        "log negative real returns principal complex logarithm"
    )){
        return 1;
    }
    if (expect_complex_value_near(
        "(expt -1 1/2)",
        0.0,
        1.0,
        "expt negative base with fractional exponent returns complex result"
    )){
        return 1;
    }
    return expect_complex_value_near(
        "(expt 2 1+1i)",
        2.0 * cos(log(2.0)),
        2.0 * sin(log(2.0)),
        "expt accepts complex exponent"
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

    RUN_TEST(test_send_primitive_uses_direct_method_dictionary);
    RUN_TEST(test_send_primitive_uses_precedence_lookup_and_cache);
    RUN_TEST(test_environment_invocation_context_lookup_walks_parent_frames);
    RUN_TEST(test_send_passes_invocation_context_kind_to_primitive_method);
    RUN_TEST(test_send_passes_next_precedence_tail_as_invocation_context_data);
    RUN_TEST(test_super_send_c_api_uses_explicit_precedence_list);
    RUN_TEST(test_class_add_method_invalidates_method_cache);
    RUN_TEST(test_immutable_list_interops_with_pairs);
    RUN_TEST(test_immutable_list_methods);
    RUN_TEST(test_immutable_list_from_list);
    RUN_TEST(test_source_location_class);
    RUN_TEST(test_immutable_list_trailer_values);
    RUN_TEST(test_immutable_list_missing_trailer_values_are_nil);
    RUN_TEST(test_list_at_c_api);
    RUN_TEST(test_list_map_single_c_api);
    RUN_TEST(test_list_map_many_c_api);
    RUN_TEST(test_list_for_each_c_api);
    RUN_TEST(test_list_any_every_c_api);
    RUN_TEST(test_define_package_special_form);
    RUN_TEST(test_in_package_special_form);
    RUN_TEST(test_use_package_special_form);
    RUN_TEST(test_use_package_with_nickname);
    RUN_TEST(test_lambda_macro_expands_to_named_nil_closure);
    RUN_TEST(test_define_function_shorthand_sets_closure_name);
    RUN_TEST(test_closure_debug_print_includes_name);
    RUN_TEST(test_anonymous_closure_debug_print_includes_address);
    RUN_TEST(test_symbol_uninterned_and_gensym_c_api);
    RUN_TEST(test_gensym_primitive);
    RUN_TEST(test_symbol_class_methods_for_uninterned_and_gensym);
    RUN_TEST(test_define_method_macro);
    RUN_TEST(test_compiler_fold_non_constant_symbol_is_unchanged);
    RUN_TEST(test_compiler_fold_application_folds_operator_and_arguments);
    RUN_TEST(test_compiler_fold_special_form_uses_special_form_reference);
    RUN_TEST(test_compiler_fold_constant_pair_is_quoted_expression);
    RUN_TEST(test_compiler_expression_constant_value_from_quote_expression);
    RUN_TEST(test_compiler_fold_expands_macros);
    RUN_TEST(test_compiler_macroexpand_preserves_expansion_chain);
    RUN_TEST(test_compiler_fold_pure_primitive_constant_folds);
    RUN_TEST(test_compiler_fold_impure_primitive_is_not_constant_folded);
    RUN_TEST(test_compiler_fold_quasiquote_folds_unquote_expression);
    RUN_TEST(test_compiler_fold_quasiquote_preserves_nested_unquote_depth);
    RUN_TEST(test_macroexpand_special_form);
    RUN_TEST(test_fold_expression_special_form);
    RUN_TEST(test_get_current_environment_special_form);
    RUN_TEST(test_symbol_class_inherits_object);
    RUN_TEST(test_symbol_package_slot_is_readonly);
    RUN_TEST(test_precedence_list_initialized);
    RUN_TEST(test_value_is_instance_of_uses_precedence_list);
    RUN_TEST(test_complex_transcendentals);
    RUN_TEST(test_boolean_constants);
    RUN_TEST(test_character_api_uses_unicode_codepoints);

#undef RUN_TEST

    if (failures == 0){
        puts("c_api tests passed");
        return 0;
    }

    fprintf(stderr, "%d c_api test(s) failed\n", failures);
    return 1;
}
