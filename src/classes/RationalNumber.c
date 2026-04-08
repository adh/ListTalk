/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/RationalNumber.h>
#include <ListTalk/classes/RealNumber.h>
#include <ListTalk/macros/decl_macros.h>

struct LT_RationalNumber_s {
    LT_Object base;
};

LT_DEFINE_CLASS(LT_RationalNumber) {
    .superclass = &LT_RealNumber_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "RationalNumber",
    .instance_size = sizeof(LT_RationalNumber),
    .class_flags = LT_CLASS_FLAG_ABSTRACT
        | LT_CLASS_FLAG_IMMUTABLE
        | LT_CLASS_FLAG_SCALAR,
};
