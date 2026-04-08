/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/ComplexNumber.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/macros/decl_macros.h>

struct LT_ComplexNumber_s {
    LT_Object base;
};

LT_DEFINE_CLASS(LT_ComplexNumber) {
    .superclass = &LT_Number_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "ComplexNumber",
    .instance_size = sizeof(LT_ComplexNumber),
    .class_flags = LT_CLASS_FLAG_ABSTRACT
        | LT_CLASS_FLAG_IMMUTABLE
        | LT_CLASS_FLAG_SCALAR,
};
