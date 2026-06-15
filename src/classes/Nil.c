/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Nil.h>
#include <ListTalk/classes/Iterator.h>
#include <ListTalk/classes/List.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/Class.h>

static size_t Nil_hash(LT_Value obj){
    (void)obj;
    return (size_t)0;
}

static int Nil_equal_p(LT_Value left, LT_Value right){
    (void)left;
    return right == LT_NIL;
}

static void Nil_debugPrintOn(LT_Value obj, FILE* stream){
    (void)obj;
    fputs("()", stream);
}

LT_DEFINE_PRIMITIVE(
    nil_method_as_iterator,
    "Nil>>asIterator",
    "(self)",
    "Return the empty iterator singleton."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    (void)self;
    return (LT_Value)(uintptr_t)LT_EmptyIterator_instance();
}

static LT_Method_Descriptor Nil_methods[] = {
    {"asIterator", &nil_method_as_iterator},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Nil) {
    .superclass = &LT_List_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Nil",
    .documentation = "The distinguished empty-list and falsey nil value.",
    .instance_size = sizeof(LT_Object),
    .class_flags = LT_CLASS_FLAG_SPECIAL | LT_CLASS_FLAG_IMMUTABLE,
    .hash = Nil_hash,
    .equal_p = Nil_equal_p,
    .debugPrintOn = Nil_debugPrintOn,
    .methods = Nil_methods,
};
