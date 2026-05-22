/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Function.h>
#include <ListTalk/macros/method_macros.h>

LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    function_method_documentation,
    "Function>>documentation",
    "Return function documentation."
)

LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    function_method_arguments,
    "Function>>arguments",
    "Return function argument documentation."
)

static LT_Method_Descriptor Function_methods[] = {
    {"documentation", &function_method_documentation},
    {"arguments", &function_method_arguments},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Function) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Function",
    .documentation = "Abstract root for callable functions.",
    .instance_size = sizeof(LT_Object),
    .class_flags = LT_CLASS_FLAG_ABSTRACT | LT_CLASS_FLAG_SPECIAL,
    .methods = Function_methods,
};
