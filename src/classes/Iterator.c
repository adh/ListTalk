/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
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
    iterator_method_has_this,
    "Iterator>>hasThis?",
    "Return true when the iterator has a current value."
)

LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    iterator_method_next,
    "Iterator>>next",
    "Advance the iterator."
)

static LT_Method_Descriptor Iterator_methods[] = {
    {"this", &iterator_method_this},
    {"hasThis?", &iterator_method_has_this},
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

LT_Value LT_Iterator_this(LT_Value iterator){
    return LT_SEND(iterator, "this");
}

LT_Value LT_Iterator_hasThis(LT_Value iterator){
    return LT_SEND(iterator, "hasThis?");
}

LT_Value LT_Iterator_next(LT_Value iterator){
    return LT_SEND(iterator, "next");
}
