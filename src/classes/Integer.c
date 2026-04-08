/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Integer.h>
#include <ListTalk/classes/RationalNumber.h>
#include <ListTalk/macros/decl_macros.h>

struct LT_Integer_s {
    LT_Object base;
};

LT_DEFINE_CLASS(LT_Integer) {
    .superclass = &LT_RationalNumber_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Integer",
    .instance_size = sizeof(LT_Integer),
    .class_flags = LT_CLASS_FLAG_ABSTRACT
        | LT_CLASS_FLAG_IMMUTABLE
        | LT_CLASS_FLAG_SCALAR,
};
