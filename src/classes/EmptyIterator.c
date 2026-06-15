/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/EmptyIterator.h>
#include <ListTalk/classes/Iterator.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/vm/error.h>

struct LT_EmptyIterator_s {
    LT_Object base;
};

static LT_EmptyIterator empty_iterator_instance = {
    .base = {.klass = &LT_EmptyIterator_class},
};

static void EmptyIterator_debugPrintOn(LT_Value obj, FILE* stream){
    (void)obj;
    fputs("#<EmptyIterator>", stream);
}

LT_DEFINE_PRIMITIVE(
    empty_iterator_method_this,
    "EmptyIterator>>this",
    "(self)",
    "Signal that an empty iterator has no current value."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    (void)self;
    LT_error("EmptyIterator has no current value");
}

LT_DEFINE_PRIMITIVE(
    empty_iterator_method_has_this,
    "EmptyIterator>>hasThis?",
    "(self)",
    "Return false."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    (void)self;
    return LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    empty_iterator_method_next,
    "EmptyIterator>>next",
    "(self)",
    "Return receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return self;
}

static LT_Method_Descriptor EmptyIterator_methods[] = {
    {"this", &empty_iterator_method_this},
    {"hasThis?", &empty_iterator_method_has_this},
    {"next", &empty_iterator_method_next},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_EmptyIterator) {
    .superclass = &LT_Iterator_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "EmptyIterator",
    .documentation = "Singleton iterator with no current value.",
    .instance_size = sizeof(LT_EmptyIterator),
    .debugPrintOn = EmptyIterator_debugPrintOn,
    .methods = EmptyIterator_methods,
};

LT_EmptyIterator* LT_EmptyIterator_instance(void){
    return &empty_iterator_instance;
}
