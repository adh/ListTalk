/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Object.h>
#include <ListTalk/macros/decl_macros.h>

LT_DEFINE_CLASS(LT_Object) {
    .superclass = NULL,
    .metaclass_superclass = &LT_Class_class,
    .name = "Object",
    .instance_size = sizeof(LT_Object),
    .class_flags = LT_CLASS_FLAG_ABSTRACT,
};
