/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/vm/error.h>

#include <stdio.h>
#include <stdlib.h>

void LT_error(const char* message, ...) {
    fprintf(stderr, "Error: %s\n", message);
    abort();
}

void LT_type_error(LT_Value value, LT_Class* expected_class){
    (void)value;
    (void)expected_class;
    LT_error("Type error");
}
