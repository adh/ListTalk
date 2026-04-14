/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Condition.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Reader.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/conditions.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/stack_trace.h>
#include <ListTalk/vm/throw_catch.h>

#include <stdio.h>
#include <stdlib.h>
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

static int g_inner_calls = 0;
static int g_outer_calls = 0;
static int g_order_index = 0;
static int g_order[8];
static LT_Value g_seen_condition_inner = LT_NIL;
static LT_Value g_seen_condition_outer = LT_NIL;
static LT_Value g_error_test_tag = LT_NIL;
static LT_Value g_backtrace_test_tag = LT_NIL;
static char* g_backtrace_output = NULL;
static size_t g_backtrace_output_length = 0;

static LT_Value read_one_with_source_file(const char* source, const char* source_file){
    LT_Reader* reader = LT_Reader_new(
        (LT_Value)(uintptr_t)LT_String_new_cstr((char*)source_file)
    );
    LT_ReaderStream* stream = LT_ReaderStream_newForString(source);
    return LT_Reader_readObject(reader, stream);
}

static void reset_state(void){
    g_inner_calls = 0;
    g_outer_calls = 0;
    g_order_index = 0;
    g_seen_condition_inner = LT_NIL;
    g_seen_condition_outer = LT_NIL;
}

static void reset_backtrace_output(void){
    free(g_backtrace_output);
    g_backtrace_output = NULL;
    g_backtrace_output_length = 0;
}

static LT_Value inner_handler_impl(LT_Value arguments,
                                   LT_Value invocation_context_kind,
                                   LT_Value invocation_context_data,
                                   LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;
    LT_Value condition;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, condition);
    LT_ARG_END(cursor);

    g_inner_calls += 1;
    g_seen_condition_inner = condition;
    g_order[g_order_index] = 1;
    g_order_index += 1;
    return LT_NIL;
}

static LT_Value outer_handler_impl(LT_Value arguments,
                                   LT_Value invocation_context_kind,
                                   LT_Value invocation_context_data,
                                   LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;
    LT_Value condition;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, condition);
    LT_ARG_END(cursor);

    g_outer_calls += 1;
    g_seen_condition_outer = condition;
    g_order[g_order_index] = 2;
    g_order_index += 1;
    return LT_NIL;
}

static LT_Value throwing_handler_impl(LT_Value arguments,
                                      LT_Value invocation_context_kind,
                                      LT_Value invocation_context_data,
                                      LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;
    LT_Value condition;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, condition);
    LT_ARG_END(cursor);
    LT_throw(condition, LT_TRUE);
}

static LT_Value catch_error_handler_impl(LT_Value arguments,
                                         LT_Value invocation_context_kind,
                                         LT_Value invocation_context_data,
                                         LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;
    LT_Value condition;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, condition);
    LT_ARG_END(cursor);
    LT_throw(g_error_test_tag, condition);
}

static LT_Value capture_backtrace_handler_impl(LT_Value arguments,
                                               LT_Value invocation_context_kind,
                                               LT_Value invocation_context_data,
                                               LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;
    LT_Value condition;
    FILE* stream;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, condition);
    LT_ARG_END(cursor);

    reset_backtrace_output();
    stream = open_memstream(&g_backtrace_output, &g_backtrace_output_length);
    if (stream == NULL){
        LT_error("Unable to allocate backtrace capture stream");
    }
    LT_stack_trace_print(stream);
    fclose(stream);

    LT_throw(g_backtrace_test_tag, condition);
}

static int test_signal_invokes_bound_handler_with_condition_value(void){
    LT_Value condition = LT_Symbol_new("condition-a");
    LT_Value inner_handler = LT_Primitive_new(
        "inner-handler",
        "(condition)",
        "condition test handler",
        inner_handler_impl
    );

    reset_state();
    LT_HANDLER_BIND(inner_handler, {
        LT_signal(condition);
    });

    if (expect(g_inner_calls == 1, "bound handler is called once")){
        return 1;
    }
    return expect(
        g_seen_condition_inner == condition,
        "handler receives signaled condition value"
    );
}

static int test_signal_runs_all_handlers_inside_out(void){
    LT_Value condition = LT_Symbol_new("condition-b");
    LT_Value inner_handler = LT_Primitive_new(
        "inner-handler",
        "(condition)",
        "condition test handler",
        inner_handler_impl
    );
    LT_Value outer_handler = LT_Primitive_new(
        "outer-handler",
        "(condition)",
        "condition test handler",
        outer_handler_impl
    );

    reset_state();
    LT_HANDLER_BIND(outer_handler, {
        LT_HANDLER_BIND(inner_handler, {
            LT_signal(condition);
        });
    });

    if (expect(g_inner_calls == 1, "inner handler called")){
        return 1;
    }
    if (expect(g_outer_calls == 1, "outer handler called")){
        return 1;
    }
    if (expect(g_order_index == 2, "both handlers recorded order")){
        return 1;
    }
    if (expect(g_order[0] == 1 && g_order[1] == 2, "handlers run inside-out")){
        return 1;
    }
    if (expect(g_seen_condition_inner == condition, "inner sees condition value")){
        return 1;
    }
    return expect(g_seen_condition_outer == condition, "outer sees condition value");
}

static int test_handler_bind_scope_is_removed_after_body(void){
    LT_Value condition = LT_Symbol_new("condition-c");
    LT_Value inner_handler = LT_Primitive_new(
        "inner-handler",
        "(condition)",
        "condition test handler",
        inner_handler_impl
    );

    reset_state();
    LT_HANDLER_BIND(inner_handler, {
        LT_signal(condition);
    });
    LT_signal(condition);

    return expect(
        g_inner_calls == 1,
        "handler only active during dynamic extent of handler-bind"
    );
}

static int test_handler_bind_scope_is_removed_on_non_local_exit(void){
    LT_Value tag = LT_Symbol_new("condition-tag");
    LT_Value caught = LT_FALSE;
    LT_Value throwing_handler = LT_Primitive_new(
        "throwing-handler",
        "(condition)",
        "throws condition as tag",
        throwing_handler_impl
    );

    reset_state();
    LT_CATCH(tag, caught, {
        LT_HANDLER_BIND(throwing_handler, {
            LT_signal(tag);
        });
    });
    LT_signal(tag);

    if (expect(caught == LT_TRUE, "handler non-local exit was caught")){
        return 1;
    }
    return expect(
        g_inner_calls == 0 && g_outer_calls == 0,
        "handler stack restored after non-local exit"
    );
}

static int test_lt_error_signals_condition_to_handlers(void){
    LT_Value caught = LT_NIL;
    LT_Value handler = LT_Primitive_new(
        "catch-error-handler",
        "(condition)",
        "captures LT_error condition",
        catch_error_handler_impl
    );

    g_error_test_tag = LT_Symbol_new("error-test-tag");
    LT_CATCH(g_error_test_tag, caught, {
        LT_HANDLER_BIND(handler, {
            LT_error("condition-probe");
        });
    });

    if (expect(LT_Value_class(caught) == &LT_Error_class, "LT_error emits Error condition")){
        return 1;
    }
    if (expect(
        strcmp(
            LT_String_value_cstr(
                LT_String_from_value(
                    LT_Object_slot_ref(caught, LT_Symbol_new("message"))
                )
            ),
            "condition-probe"
        ) == 0,
        "LT_error forwards condition message through LT_signal"
    )){
        return 1;
    }
    return expect(
        LT_Object_slot_ref(caught, LT_Symbol_new("args")) == LT_NIL,
        "LT_error emits empty argument list by default"
    );
}

static int test_backtrace_prints_source_locations_and_expansion_chain(void){
    LT_Environment* env = LT_new_base_environment();
    LT_Value caught = LT_NIL;
    LT_Value handler = LT_Primitive_new(
        "capture-backtrace-handler",
        "(condition)",
        "captures printed backtrace",
        capture_backtrace_handler_impl
    );

    reset_backtrace_output();
    g_backtrace_test_tag = LT_Symbol_new("backtrace-test-tag");

    (void)LT_eval(
        read_one_with_source_file(
            "(define-macro (boom) '(+ missing 1))",
            "fixtures/macros.lt"
        ),
        env,
        NULL
    );

    LT_CATCH(g_backtrace_test_tag, caught, {
        LT_HANDLER_BIND(handler, {
            (void)LT_eval(
                read_one_with_source_file("(boom)", "fixtures/main.lt"),
                env,
                NULL
            );
        });
    });

    if (expect(
        LT_Value_class(caught) == &LT_Error_class,
        "backtrace test catches signaled error condition"
    )){
        return 1;
    }
    if (expect(g_backtrace_output != NULL, "backtrace test captured backtrace output")){
        return 1;
    }
    if (expect(
        strstr(g_backtrace_output, "Backtrace:\n") != NULL,
        "printed backtrace starts with header"
    )){
        return 1;
    }
    if (expect(
        strstr(g_backtrace_output, "eval expr=(+ missing 1)\n") != NULL,
        "printed backtrace includes expanded frame expression"
    )){
        return 1;
    }
    if (expect(
        strstr(g_backtrace_output, "    at fixtures/macros.lt:1:23\n") != NULL,
        "printed backtrace includes source location for expanded form"
    )){
        return 1;
    }
    if (expect(
        strstr(g_backtrace_output, "    expanded from (boom)\n") != NULL,
        "printed backtrace includes expansion chain"
    )){
        return 1;
    }
    return expect(
        strstr(g_backtrace_output, "      at fixtures/main.lt:1:1\n") != NULL,
        "printed backtrace indents original source location under expansion chain"
    );
}

static int test_error_builder_collects_named_arguments(void){
    LT_Value condition = LT_Error(
        "structured-error",
        "operator", LT_Symbol_new("quasiquote"),
        "form", LT_Symbol_new("unquote"),
        NULL
    );
    LT_Value args = LT_Object_slot_ref(condition, LT_Symbol_new("args"));

    if (expect(LT_Value_class(condition) == &LT_Error_class, "LT_Error builds Error condition")){
        return 1;
    }
    if (expect(LT_Pair_p(args), "LT_Error stores argument plist")){
        return 1;
    }
    if (expect(LT_car(args) == LT_Symbol_new("operator"), "LT_Error stores first key as symbol")){
        return 1;
    }
    if (expect(LT_car(LT_cdr(args)) == LT_Symbol_new("quasiquote"), "LT_Error stores first value")){
        return 1;
    }
    if (expect(LT_car(LT_cdr(LT_cdr(args))) == LT_Symbol_new("form"), "LT_Error stores second key")){
        return 1;
    }
    return expect(
        LT_car(LT_cdr(LT_cdr(LT_cdr(args)))) == LT_Symbol_new("unquote"),
        "LT_Error stores second value"
    );
}

int main(void){
    int failures = 0;

    LT_init();

    failures += test_signal_invokes_bound_handler_with_condition_value();
    failures += test_signal_runs_all_handlers_inside_out();
    failures += test_handler_bind_scope_is_removed_after_body();
    failures += test_handler_bind_scope_is_removed_on_non_local_exit();
    failures += test_lt_error_signals_condition_to_handlers();
    failures += test_backtrace_prints_source_locations_and_expansion_chain();
    failures += test_error_builder_collects_named_arguments();

    if (failures == 0){
        puts("conditions tests passed");
        return 0;
    }

    fprintf(stderr, "%d conditions test(s) failed\n", failures);
    return 1;
}
