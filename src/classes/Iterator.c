/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Iterator.h>
#include <ListTalk/classes/List.h>
#include <ListTalk/classes/Object.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/macros/method_macros.h>
#include <ListTalk/vm/error.h>

struct LT_EmptyIterator_s {
    LT_Object base;
};

struct LT_ListIterator_s {
    LT_Object base;
    LT_Value current;
    LT_Value rest;
};

static LT_EmptyIterator empty_iterator_instance = {
    .base = {.klass = &LT_EmptyIterator_class},
};

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

static void list_iterator_validate_rest(LT_ListIterator* iterator){
    if (iterator->rest != LT_NIL && !LT_Pair_p(iterator->rest)){
        LT_error("ListIterator expects proper list");
    }
}

static void list_iterator_advance(LT_ListIterator* iterator){
    if (iterator->rest == LT_NIL){
        iterator->current = LT_INVALID;
        return;
    }
    if (!LT_Pair_p(iterator->rest)){
        LT_error("ListIterator expects proper list");
    }

    iterator->current = LT_car(iterator->rest);
    iterator->rest = LT_cdr(iterator->rest);
}

static void ListIterator_debugPrintOn(LT_Value obj, FILE* stream){
    LT_ListIterator* iterator = LT_ListIterator_from_value(obj);

    fputs("#<ListIterator current=", stream);
    if (iterator->current == LT_INVALID){
        fputs("#<invalid>", stream);
    } else {
        LT_Value_debugPrintOn(iterator->current, stream);
    }
    fputs(" rest=", stream);
    LT_Value_debugPrintOn(iterator->rest, stream);
    fputc('>', stream);
}

LT_DEFINE_PRIMITIVE(
    list_iterator_method_this,
    "ListIterator>>this",
    "(self)",
    "Return the current value of the iterator."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_ListIterator* iterator;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    iterator = LT_ListIterator_from_value(self);
    if (iterator->current == LT_INVALID){
        LT_error("ListIterator is not positioned");
    }
    return iterator->current;
}

LT_DEFINE_PRIMITIVE(
    list_iterator_method_has_this,
    "ListIterator>>hasThis?",
    "(self)",
    "Return true when the iterator has a current value."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_ListIterator* iterator;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    iterator = LT_ListIterator_from_value(self);
    list_iterator_validate_rest(iterator);
    return iterator->current == LT_INVALID ? LT_FALSE : LT_TRUE;
}

LT_DEFINE_PRIMITIVE(
    list_iterator_method_next,
    "ListIterator>>next",
    "(self)",
    "Advance the iterator and return receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_ListIterator* iterator;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    iterator = LT_ListIterator_from_value(self);
    list_iterator_advance(iterator);
    return self;
}

static LT_Method_Descriptor ListIterator_methods[] = {
    {"this", &list_iterator_method_this},
    {"hasThis?", &list_iterator_method_has_this},
    {"next", &list_iterator_method_next},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_ListIterator) {
    .superclass = &LT_Iterator_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "ListIterator",
    .documentation = "Iterator over proper list elements.",
    .instance_size = sizeof(LT_ListIterator),
    .debugPrintOn = ListIterator_debugPrintOn,
    .methods = ListIterator_methods,
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

LT_EmptyIterator* LT_EmptyIterator_instance(void){
    return &empty_iterator_instance;
}

LT_ListIterator* LT_ListIterator_new(LT_Value list){
    LT_ListIterator* iterator;

    if (!LT_List_p(list)){
        LT_type_error(list, &LT_List_class);
    }

    iterator = LT_Class_ALLOC(LT_ListIterator);
    iterator->current = LT_INVALID;
    iterator->rest = list;
    list_iterator_advance(iterator);
    return iterator;
}
