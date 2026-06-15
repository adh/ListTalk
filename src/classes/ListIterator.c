/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Iterator.h>
#include <ListTalk/classes/List.h>
#include <ListTalk/classes/ListIterator.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/vm/error.h>

struct LT_ListIterator_s {
    LT_Object base;
    LT_Value current;
    LT_Value rest;
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
