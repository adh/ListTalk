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

struct LT_MapIterator_s {
    LT_Object base;
    LT_Value iterator;
    LT_Value callable;
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

LT_DEFINE_PRIMITIVE(
    iterator_method_as_list,
    "Iterator>>asList",
    "(self)",
    "Consume iterator and return remaining values as a list."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_ListBuilder* builder;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    builder = LT_ListBuilder_new();
    while (LT_Value_truthy_p(LT_Iterator_hasThis(self))){
        LT_ListBuilder_append(builder, LT_Iterator_this(self));
        (void)LT_Iterator_next(self);
    }
    return LT_ListBuilder_value(builder);
}

static LT_Method_Descriptor Iterator_methods[] = {
    {"this", &iterator_method_this},
    {"hasThis?", &iterator_method_has_this},
    {"next", &iterator_method_next},
    {"asList", &iterator_method_as_list},
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

static void MapIterator_debugPrintOn(LT_Value obj, FILE* stream){
    LT_MapIterator* iterator = LT_MapIterator_from_value(obj);

    fputs("#<MapIterator iterator=", stream);
    LT_Value_debugPrintOn(iterator->iterator, stream);
    fputs(" callable=", stream);
    LT_Value_debugPrintOn(iterator->callable, stream);
    fputc('>', stream);
}

LT_DEFINE_PRIMITIVE(
    map_iterator_class_method_map_with,
    "MapIterator class>>map:with:",
    "(self iterator callable)",
    "Return a lazy iterator mapping callable over iterator."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value iterator;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, iterator);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_MapIterator_class){
        LT_error("map:with: class method is only supported on MapIterator");
    }
    return (LT_Value)(uintptr_t)LT_MapIterator_new(iterator, callable);
}

LT_DEFINE_PRIMITIVE(
    map_iterator_method_this,
    "MapIterator>>this",
    "(self)",
    "Return callable applied to wrapped iterator current value."
){
    LT_Value cursor = arguments;
    LT_MapIterator* iterator;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, iterator, LT_MapIterator*, LT_MapIterator_from_value);
    LT_ARG_END(cursor);
    return LT_apply(
        iterator->callable,
        LT_cons(LT_Iterator_this(iterator->iterator), LT_NIL),
        LT_NIL,
        LT_NIL,
        NULL
    );
}

LT_DEFINE_PRIMITIVE(
    map_iterator_method_has_this,
    "MapIterator>>hasThis?",
    "(self)",
    "Return true when the wrapped iterator has a current value."
){
    LT_Value cursor = arguments;
    LT_MapIterator* iterator;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, iterator, LT_MapIterator*, LT_MapIterator_from_value);
    LT_ARG_END(cursor);
    return LT_Iterator_hasThis(iterator->iterator);
}

LT_DEFINE_PRIMITIVE(
    map_iterator_method_next,
    "MapIterator>>next",
    "(self)",
    "Advance the wrapped iterator and return receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_MapIterator* iterator;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    iterator = LT_MapIterator_from_value(self);
    (void)LT_Iterator_next(iterator->iterator);
    return self;
}

static LT_Method_Descriptor MapIterator_methods[] = {
    {"this", &map_iterator_method_this},
    {"hasThis?", &map_iterator_method_has_this},
    {"next", &map_iterator_method_next},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor MapIterator_class_methods[] = {
    {"map:with:", &map_iterator_class_method_map_with},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_MapIterator) {
    .superclass = &LT_Iterator_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "MapIterator",
    .documentation = "Lazy iterator mapping a callable over another iterator.",
    .instance_size = sizeof(LT_MapIterator),
    .debugPrintOn = MapIterator_debugPrintOn,
    .methods = MapIterator_methods,
    .class_methods = MapIterator_class_methods,
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

LT_MapIterator* LT_MapIterator_new(LT_Value iterator, LT_Value callable){
    LT_MapIterator* map_iterator = LT_Class_ALLOC(LT_MapIterator);

    map_iterator->iterator = iterator;
    map_iterator->callable = callable;
    return map_iterator;
}
