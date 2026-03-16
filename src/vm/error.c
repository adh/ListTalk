/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/vm/error.h>
#include <ListTalk/vm/conditions.h>
#include <ListTalk/vm/stack_trace.h>
#include <ListTalk/classes/String.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void LT_print_backtrace(FILE* stream){
    LT_stack_trace_print(stream);
}

void _Noreturn LT_error(const char* message, ...) {
    LT_Value condition = (LT_Value)(uintptr_t)LT_String_new_cstr((char*)message);
    LT_signal(condition);
    fprintf(stderr, "Unrecoverable error: %s\n", message);
    LT_print_backtrace(stderr);
#ifdef __APPLE__
    _exit(1); /* Use _exit on macOS to avoid Crash Reporter */
#else
    abort();
#endif
}

void LT_type_error(LT_Value value, LT_Class* expected_class){
    (void)value;
    (void)expected_class;
    LT_error("Type error");
}
