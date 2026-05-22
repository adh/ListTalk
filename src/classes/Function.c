/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/CompoundForm.h>
#include <ListTalk/classes/Function.h>

LT_DEFINE_CLASS(LT_Function) {
    .superclass = &LT_CompoundForm_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Function",
    .documentation = "Abstract root for callable functions.",
    .instance_size = sizeof(LT_Object),
    .class_flags = LT_CLASS_FLAG_ABSTRACT | LT_CLASS_FLAG_SPECIAL,
};
