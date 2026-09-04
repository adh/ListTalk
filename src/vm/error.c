/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Condition.h>
#include <ListTalk/classes/Restart.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/conditions.h>
#include <ListTalk/vm/stack_trace.h>
#include <ListTalk/vm/throw_catch.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

static LT_Value cerror_continue_tag(void){
    return LT_Symbol_new_in(
        LT_PACKAGE_LISTTALK_IMPLEMENTATION,
        "cerror-continue"
    );
}

LT_DEFINE_PRIMITIVE_RESTART(
    cerror_continue_restart,
    "continue",
    "()",
    "Continue from the correctable error."
){
    LT_Value cursor = arguments;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_ARG_END(cursor);
    LT_throw(cerror_continue_tag(), LT_TRUE);
}

void LT_print_backtrace(FILE* stream){
    LT_stack_trace_print(stream);
}

void _Noreturn LT_error_impl(const char* message, ...) {
    LT_Value condition;
    va_list args;

    va_start(args, message);
    condition = LT_Condition_vnew(&LT_Error_class, message, args);
    va_end(args);
    LT_signal(condition);
    LT_invoke_debugger(condition);
    fprintf(stderr, "Unrecoverable error: %s\n", message);
    LT_print_backtrace(stderr);
#ifdef __APPLE__
    _exit(1); /* Use _exit on macOS to avoid Crash Reporter */
#else
    abort();
#endif
}

void LT_cerror_impl(const char* message, ...){
    LT_Value condition;
    LT_Value continued = LT_NIL;
    va_list args;

    va_start(args, message);
    condition = LT_Condition_vnew(&LT_Error_class, message, args);
    va_end(args);

    LT_CATCH(cerror_continue_tag(), continued, {
        LT_RESTART_BIND(LT_Restart_from_static(&cerror_continue_restart), {
            LT_signal(condition);
            LT_invoke_debugger(condition);
            fprintf(stderr, "Unrecoverable error: %s\n", message);
            LT_print_backtrace(stderr);
#ifdef __APPLE__
            _exit(1); /* Use _exit on macOS to avoid Crash Reporter */
#else
            abort();
#endif
        });
    });
    (void)continued;
}

void _Noreturn LT_system_error(const char* message, int errnum){
    LT_Value condition = LT_SystemError_new(message, errnum, LT_NIL);

    LT_signal(condition);
    LT_invoke_debugger(condition);
    fprintf(
        stderr,
        "Unrecoverable system error: %s: %s\n",
        message,
        LT_strerror(errnum)
    );
    LT_print_backtrace(stderr);
#ifdef __APPLE__
    _exit(1);
#else
    abort();
#endif
}

void _Noreturn LT_subclass_responsibility_error(void){
    static const char* message = "Subclass responsibility";
    LT_Value condition = LT_SubclassResponsibilityError(message);

    LT_signal(condition);
    LT_invoke_debugger(condition);
    fprintf(stderr, "Unrecoverable subclass responsibility error: %s\n", message);
    LT_print_backtrace(stderr);
#ifdef __APPLE__
    _exit(1);
#else
    abort();
#endif
}

void LT_type_error(LT_Value value, LT_Class* expected_class){
    LT_error(
        "Type error",
        "value", value,
        "expected-class", (LT_Value)(uintptr_t)expected_class,
        NULL
    );
}
