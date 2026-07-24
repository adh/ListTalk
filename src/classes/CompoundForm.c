/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/CompoundForm.h>
#include <ListTalk/macros/method_macros.h>

LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    compound_form_method_documentation,
    "CompoundForm>>documentation",
    "Return compound form documentation."
)

LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    compound_form_method_arguments,
    "CompoundForm>>arguments",
    "Return compound form argument documentation."
)

static LT_Method_Descriptor CompoundForm_methods[] = {
    {"documentation", &compound_form_method_documentation},
    {"arguments", &compound_form_method_arguments},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_CompoundForm) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "CompoundForm",
    .documentation = "Abstract root for compound syntactic and callable forms.",
    .instance_size = sizeof(LT_Object),
    .class_flags = LT_CLASS_FLAG_ABSTRACT | LT_CLASS_FLAG_SPECIAL,
    .methods = CompoundForm_methods,
};
