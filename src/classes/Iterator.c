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

struct LT_FilterIterator_s {
    LT_Object base;
    LT_Value iterator;
    LT_Value callable;
};

static LT_EmptyIterator empty_iterator_instance = {
    .base = {.klass = &LT_EmptyIterator_class},
};

static LT_Value iterator_apply1(LT_Value callable, LT_Value value){
    return LT_apply(
        callable,
        LT_cons(value, LT_NIL),
        LT_NIL,
        LT_NIL,
        NULL
    );
}

static LT_Value iterator_apply2(LT_Value callable,
                                LT_Value left,
                                LT_Value right){
    return LT_apply(
        callable,
        LT_cons(left, LT_cons(right, LT_NIL)),
        LT_NIL,
        LT_NIL,
        NULL
    );
}

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
    "Iterator>>next!",
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

LT_DEFINE_PRIMITIVE(
    iterator_method_as_iterator,
    "Iterator>>asIterator",
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

LT_DEFINE_PRIMITIVE(
    iterator_method_map,
    "Iterator>>map:",
    "(self callable)",
    "Return a lazy iterator mapping callable over receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_MapIterator_new(self, callable);
}

LT_DEFINE_PRIMITIVE(
    iterator_method_filter,
    "Iterator>>filter:",
    "(self callable)",
    "Return a lazy iterator selecting values for which callable is truthy."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_FilterIterator_new(self, callable);
}

LT_DEFINE_PRIMITIVE(
    iterator_method_for_each,
    "Iterator>>forEach:",
    "(self callable)",
    "Consume receiver, applying callable to each value, and return nil."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);
    while (LT_Value_truthy_p(LT_Iterator_hasThis(self))){
        (void)iterator_apply1(callable, LT_Iterator_this(self));
        (void)LT_Iterator_next(self);
    }
    return LT_NIL;
}

LT_DEFINE_PRIMITIVE(
    iterator_method_any,
    "Iterator>>any:",
    "(self callable)",
    "Consume receiver until callable returns truthy or it is exhausted."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);
    while (LT_Value_truthy_p(LT_Iterator_hasThis(self))){
        LT_Value value = LT_Iterator_this(self);
        (void)LT_Iterator_next(self);
        if (LT_Value_truthy_p(iterator_apply1(callable, value))){
            return LT_TRUE;
        }
    }
    return LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    iterator_method_every,
    "Iterator>>every:",
    "(self callable)",
    "Consume receiver until callable returns falsey or it is exhausted."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);
    while (LT_Value_truthy_p(LT_Iterator_hasThis(self))){
        LT_Value value = LT_Iterator_this(self);
        (void)LT_Iterator_next(self);
        if (!LT_Value_truthy_p(iterator_apply1(callable, value))){
            return LT_FALSE;
        }
    }
    return LT_TRUE;
}

LT_DEFINE_PRIMITIVE(
    iterator_method_inject_into,
    "Iterator>>inject:into:",
    "(self initial callable)",
    "Consume receiver, folding values from the left."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value accumulator;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, accumulator);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);
    while (LT_Value_truthy_p(LT_Iterator_hasThis(self))){
        accumulator = iterator_apply2(callable, accumulator, LT_Iterator_this(self));
        (void)LT_Iterator_next(self);
    }
    return accumulator;
}

LT_DEFINE_PRIMITIVE(
    iterator_method_reduce,
    "Iterator>>reduce:",
    "(self callable)",
    "Consume receiver, reducing values from the left."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    LT_Value accumulator;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);
    if (!LT_Value_truthy_p(LT_Iterator_hasThis(self))){
        LT_error("Iterator reduce: requires at least one value");
    }
    accumulator = LT_Iterator_this(self);
    (void)LT_Iterator_next(self);
    while (LT_Value_truthy_p(LT_Iterator_hasThis(self))){
        accumulator = iterator_apply2(callable, accumulator, LT_Iterator_this(self));
        (void)LT_Iterator_next(self);
    }
    return accumulator;
}

static LT_Method_Descriptor Iterator_methods[] = {
    {"this", &iterator_method_this},
    {"hasThis?", &iterator_method_has_this},
    {"next!", &iterator_method_next},
    {"asList", &iterator_method_as_list},
    {"asIterator", &iterator_method_as_iterator},
    {"map:", &iterator_method_map},
    {"filter:", &iterator_method_filter},
    {"forEach:", &iterator_method_for_each},
    {"do:", &iterator_method_for_each},
    {"any:", &iterator_method_any},
    {"every:", &iterator_method_every},
    {"inject:into:", &iterator_method_inject_into},
    {"reduce:", &iterator_method_reduce},
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
    "EmptyIterator>>next!",
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
    {"next!", &empty_iterator_method_next},
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
    "ListIterator>>next!",
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
    {"next!", &list_iterator_method_next},
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
    "MapIterator>>next!",
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
    {"next!", &map_iterator_method_next},
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

static void filter_iterator_advance_to_match(LT_FilterIterator* iterator){
    while (LT_Value_truthy_p(LT_Iterator_hasThis(iterator->iterator))){
        if (LT_Value_truthy_p(iterator_apply1(
                iterator->callable,
                LT_Iterator_this(iterator->iterator)
            ))){
            return;
        }
        (void)LT_Iterator_next(iterator->iterator);
    }
}

static void FilterIterator_debugPrintOn(LT_Value obj, FILE* stream){
    LT_FilterIterator* iterator = LT_FilterIterator_from_value(obj);

    fputs("#<FilterIterator iterator=", stream);
    LT_Value_debugPrintOn(iterator->iterator, stream);
    fputs(" callable=", stream);
    LT_Value_debugPrintOn(iterator->callable, stream);
    fputc('>', stream);
}

LT_DEFINE_PRIMITIVE(
    filter_iterator_class_method_filter_with,
    "FilterIterator class>>filter:with:",
    "(self iterator callable)",
    "Return a lazy iterator filtering iterator with callable."
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
    if (self != (LT_Value)(uintptr_t)&LT_FilterIterator_class){
        LT_error("filter:with: class method is only supported on FilterIterator");
    }
    return (LT_Value)(uintptr_t)LT_FilterIterator_new(iterator, callable);
}

LT_DEFINE_PRIMITIVE(
    filter_iterator_method_this,
    "FilterIterator>>this",
    "(self)",
    "Return the current matching value."
){
    LT_Value cursor = arguments;
    LT_FilterIterator* iterator;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, iterator, LT_FilterIterator*, LT_FilterIterator_from_value);
    LT_ARG_END(cursor);
    return LT_Iterator_this(iterator->iterator);
}

LT_DEFINE_PRIMITIVE(
    filter_iterator_method_has_this,
    "FilterIterator>>hasThis?",
    "(self)",
    "Return true when a matching value remains."
){
    LT_Value cursor = arguments;
    LT_FilterIterator* iterator;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, iterator, LT_FilterIterator*, LT_FilterIterator_from_value);
    LT_ARG_END(cursor);
    return LT_Iterator_hasThis(iterator->iterator);
}

LT_DEFINE_PRIMITIVE(
    filter_iterator_method_next,
    "FilterIterator>>next!",
    "(self)",
    "Advance to the next matching value and return receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_FilterIterator* iterator;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    iterator = LT_FilterIterator_from_value(self);
    (void)LT_Iterator_next(iterator->iterator);
    filter_iterator_advance_to_match(iterator);
    return self;
}

static LT_Method_Descriptor FilterIterator_methods[] = {
    {"this", &filter_iterator_method_this},
    {"hasThis?", &filter_iterator_method_has_this},
    {"next!", &filter_iterator_method_next},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor FilterIterator_class_methods[] = {
    {"filter:with:", &filter_iterator_class_method_filter_with},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_FilterIterator) {
    .superclass = &LT_Iterator_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "FilterIterator",
    .documentation = "Lazy iterator selecting values accepted by a callable.",
    .instance_size = sizeof(LT_FilterIterator),
    .debugPrintOn = FilterIterator_debugPrintOn,
    .methods = FilterIterator_methods,
    .class_methods = FilterIterator_class_methods,
};

LT_Value LT_Iterator_this(LT_Value iterator){
    return LT_SEND(iterator, "this");
}

LT_Value LT_Iterator_hasThis(LT_Value iterator){
    return LT_SEND(iterator, "hasThis?");
}

LT_Value LT_Iterator_next(LT_Value iterator){
    return LT_SEND(iterator, "next!");
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

LT_FilterIterator* LT_FilterIterator_new(LT_Value iterator, LT_Value callable){
    LT_FilterIterator* result = LT_Class_ALLOC(LT_FilterIterator);

    result->iterator = iterator;
    result->callable = callable;
    filter_iterator_advance_to_match(result);
    return result;
}
