/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/conditions.h>
#include <ListTalk/vm/throw_catch.h>

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

static int g_inner_calls = 0;
static int g_outer_calls = 0;
static int g_order_index = 0;
static int g_order[8];
static LT_Value g_seen_condition_inner = LT_NIL;
static LT_Value g_seen_condition_outer = LT_NIL;

static void reset_state(void){
    g_inner_calls = 0;
    g_outer_calls = 0;
    g_order_index = 0;
    g_seen_condition_inner = LT_NIL;
    g_seen_condition_outer = LT_NIL;
}

static LT_Value inner_handler_impl(LT_Value arguments,
                                   LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;
    LT_Value condition;
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
                                   LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;
    LT_Value condition;
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
                                      LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Value cursor = arguments;
    LT_Value condition;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, condition);
    LT_ARG_END(cursor);
    LT_throw(condition, LT_TRUE);
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

int main(void){
    int failures = 0;

    LT_init();

    failures += test_signal_invokes_bound_handler_with_condition_value();
    failures += test_signal_runs_all_handlers_inside_out();
    failures += test_handler_bind_scope_is_removed_after_body();
    failures += test_handler_bind_scope_is_removed_on_non_local_exit();

    if (failures == 0){
        puts("conditions tests passed");
        return 0;
    }

    fprintf(stderr, "%d conditions test(s) failed\n", failures);
    return 1;
}
