/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Iterator.h>
#include <ListTalk/classes/Object.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/macros/method_macros.h>

LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    iterator_method_this,
    "Iterator>>this",
    "Return the current value of the iterator."
)

LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    iterator_method_has_next,
    "Iterator>>hasNext?",
    "Return true when the iterator can advance to another value."
)

LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    iterator_method_next,
    "Iterator>>next",
    "Advance the iterator and return the next value."
)

static LT_Method_Descriptor Iterator_methods[] = {
    {"this", &iterator_method_this},
    {"hasNext?", &iterator_method_has_next},
    {"next", &iterator_method_next},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Iterator) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Iterator",
    .documentation = "Abstract root for mutable iterators.",
    .instance_size = sizeof(LT_Object),
    .class_flags = LT_CLASS_FLAG_ABSTRACT,
    .methods = Iterator_methods,
};
