/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Condition.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/conditions.h>
#include <ListTalk/vm/stack_trace.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

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
    fprintf(stderr, "Unrecoverable error: %s\n", message);
    LT_print_backtrace(stderr);
#ifdef __APPLE__
    _exit(1); /* Use _exit on macOS to avoid Crash Reporter */
#else
    abort();
#endif
}

void _Noreturn LT_system_error(const char* message, int errnum){
    LT_Value condition = LT_SystemError_new(message, errnum, LT_NIL);

    LT_signal(condition);
    fprintf(
        stderr,
        "Unrecoverable system error: %s: %s\n",
        message,
        strerror(errnum)
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
