/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Iterator.h>
#include <ListTalk/classes/Vector.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/macros/arg_macros.h>

typedef struct VectorSortComparator_s {
    LT_Value callable;
} VectorSortComparator;

struct LT_Vector_s {
    LT_Object base;
    size_t length;
    LT_Value items[];
};

struct LT_VectorIterator_s {
    LT_Object base;
    LT_Vector* vector;
    size_t index;
};

static int sort_compare_values(LT_Value left,
                               LT_Value right,
                               VectorSortComparator* comparator){
    LT_Value result;
    int64_t comparison;

    if (comparator->callable == LT_NIL){
        result = LT_SEND(left, "compareWith:", right);
    } else {
        result = LT_apply(
            comparator->callable,
            LT_cons(left, LT_cons(right, LT_NIL)),
            LT_NIL,
            LT_NIL,
            NULL
        );
    }

    comparison = LT_SmallInteger_value(result);
    return comparison < 0 ? -1 : (comparison > 0 ? 1 : 0);
}

static void merge_sort_values(LT_Value* items,
                              LT_Value* scratch,
                              size_t from,
                              size_t to,
                              VectorSortComparator* comparator){
    size_t middle;
    size_t left;
    size_t right;
    size_t out;
    size_t i;

    if (to - from < 2){
        return;
    }

    middle = from + (to - from) / 2;
    merge_sort_values(items, scratch, from, middle, comparator);
    merge_sort_values(items, scratch, middle, to, comparator);

    left = from;
    right = middle;
    out = from;
    while (left < middle && right < to){
        if (sort_compare_values(items[left], items[right], comparator) <= 0){
            scratch[out++] = items[left++];
        } else {
            scratch[out++] = items[right++];
        }
    }
    while (left < middle){
        scratch[out++] = items[left++];
    }
    while (right < to){
        scratch[out++] = items[right++];
    }
    for (i = from; i < to; i++){
        items[i] = scratch[i];
    }
}

static void sort_values(LT_Value* items,
                        size_t length,
                        LT_Value callable){
    VectorSortComparator comparator;
    LT_Value* scratch;

    if (length < 2){
        return;
    }

    comparator.callable = callable;
    scratch = GC_MALLOC(sizeof(LT_Value) * length);
    merge_sort_values(items, scratch, 0, length, &comparator);
}

static size_t Vector_hash(LT_Value value){
    LT_Vector* vector = LT_Vector_from_value(value);
    size_t length = LT_Vector_length(vector);
    size_t hash = (size_t)0x9e3779b1;
    size_t i;

    for (i = 0; i < length; i++){
        hash = (hash * (size_t)33) ^ LT_Value_hash(vector->items[i]);
    }
    return hash;
}

static int Vector_equal_p(LT_Value left, LT_Value right){
    LT_Vector* left_vector;
    LT_Vector* right_vector;
    size_t i;
    size_t length;

    if (!LT_Vector_p(right)){
        return 0;
    }

    left_vector = LT_Vector_from_value(left);
    right_vector = LT_Vector_from_value(right);
    if (LT_Vector_length(left_vector) != LT_Vector_length(right_vector)){
        return 0;
    }

    length = LT_Vector_length(left_vector);
    for (i = 0; i < length; i++){
        if (!LT_Value_equal_p(left_vector->items[i], right_vector->items[i])){
            return 0;
        }
    }

    return 1;
}

static void Vector_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Vector* vector = LT_Vector_from_value(obj);
    size_t i;

    fputs("#(", stream);
    for (i = 0; i < vector->length; i++){
        if (i != 0){
            fputc(' ', stream);
        }
        LT_Value_debugPrintOn(vector->items[i], stream);
    }
    fputc(')', stream);
}

static void VectorIterator_debugPrintOn(LT_Value obj, FILE* stream){
    LT_VectorIterator* iterator = LT_VectorIterator_from_value(obj);

    fprintf(stream, "#<VectorIterator %p index=%zu>", (void*)iterator, iterator->index);
}

static LT_Value VectorIterator_current(LT_VectorIterator* iterator){
    if (iterator->index >= LT_Vector_length(iterator->vector)){
        LT_error("VectorIterator is not positioned");
    }
    return LT_Vector_at(iterator->vector, iterator->index);
}

LT_DEFINE_PRIMITIVE(
    vector_iterator_method_this,
    "VectorIterator>>this",
    "(self)",
    "Return the current value of the iterator."
){
    LT_Value cursor = arguments;
    LT_VectorIterator* iterator;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, iterator, LT_VectorIterator*, LT_VectorIterator_from_value);
    LT_ARG_END(cursor);
    return VectorIterator_current(iterator);
}

LT_DEFINE_PRIMITIVE(
    vector_iterator_method_has_this,
    "VectorIterator>>hasThis?",
    "(self)",
    "Return true when the iterator has a current value."
){
    LT_Value cursor = arguments;
    LT_VectorIterator* iterator;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, iterator, LT_VectorIterator*, LT_VectorIterator_from_value);
    LT_ARG_END(cursor);
    return iterator->index < LT_Vector_length(iterator->vector) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    vector_iterator_method_next,
    "VectorIterator>>next!",
    "(self)",
    "Advance the iterator and return receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_VectorIterator* iterator;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    iterator = LT_VectorIterator_from_value(self);
    if (iterator->index < LT_Vector_length(iterator->vector)){
        iterator->index++;
    }
    return self;
}

LT_DEFINE_PRIMITIVE(
    vector_method_length,
    "Vector>>length",
    "(self)",
    "Return vector length."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Number_smallinteger_from_size(
        LT_Vector_length(LT_Vector_from_value(self)),
        "Vector length does not fit fixnum"
    );
}

LT_DEFINE_PRIMITIVE(
    vector_method_at,
    "Vector>>at:",
    "(self index)",
    "Return vector item at index."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value index;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, index);
    LT_ARG_END(cursor);
    return LT_Vector_at(
        LT_Vector_from_value(self),
        LT_Number_nonnegative_size_from_integer(
            index,
            "Vector index out of bounds",
            "Vector index out of bounds"
        )
    );
}

LT_DEFINE_PRIMITIVE(
    vector_method_at_put,
    "Vector>>at:put:",
    "(self index value)",
    "Set vector item at index."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value index;
    LT_Value value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, index);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    LT_Vector_atPut(
        LT_Vector_from_value(self),
        LT_Number_nonnegative_size_from_integer(
            index,
            "Vector index out of bounds",
            "Vector index out of bounds"
        ),
        value
    );
    return value;
}

LT_DEFINE_PRIMITIVE(
    vector_method_sort,
    "Vector>>sort",
    "(self)",
    "Return a fresh vector sorted with element compareWith:."
){
    LT_Value cursor = arguments;
    LT_Vector* vector;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, vector, LT_Vector*, LT_Vector_from_value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_Vector_sort(vector);
}

LT_DEFINE_PRIMITIVE(
    vector_method_sort_using,
    "Vector>>sortUsing:",
    "(self callable)",
    "Return a fresh vector sorted with a two-argument comparator."
){
    LT_Value cursor = arguments;
    LT_Vector* vector;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, vector, LT_Vector*, LT_Vector_from_value);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_Vector_sortUsing(vector, callable);
}

LT_DEFINE_PRIMITIVE(
    vector_method_as_list,
    "Vector>>asList",
    "(self)",
    "Return vector elements as a list."
){
    LT_Value cursor = arguments;
    LT_Vector* vector;
    LT_ListBuilder* builder;
    size_t i;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, vector, LT_Vector*, LT_Vector_from_value);
    LT_ARG_END(cursor);

    builder = LT_ListBuilder_new();
    for (i = 0; i < LT_Vector_length(vector); i++){
        LT_ListBuilder_append(builder, LT_Vector_at(vector, i));
    }
    return LT_ListBuilder_value(builder);
}

LT_DEFINE_PRIMITIVE(
    vector_method_as_iterator,
    "Vector>>asIterator",
    "(self)",
    "Return an iterator over vector elements."
){
    LT_Value cursor = arguments;
    LT_Vector* vector;
    LT_VectorIterator* iterator;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, vector, LT_Vector*, LT_Vector_from_value);
    LT_ARG_END(cursor);
    if (LT_Vector_length(vector) == 0){
        return (LT_Value)(uintptr_t)LT_EmptyIterator_instance();
    }

    iterator = LT_Class_ALLOC(LT_VectorIterator);
    iterator->vector = vector;
    iterator->index = 0;
    return (LT_Value)(uintptr_t)iterator;
}

static LT_Method_Descriptor VectorIterator_methods[] = {
    {"this", &vector_iterator_method_this},
    {"hasThis?", &vector_iterator_method_has_this},
    {"next!", &vector_iterator_method_next},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor Vector_methods[] = {
    {"length", &vector_method_length},
    {"at:", &vector_method_at},
    {"at:put:", &vector_method_at_put},
    {"sort", &vector_method_sort},
    {"sortUsing:", &vector_method_sort_using},
    {"asList", &vector_method_as_list},
    {"asIterator", &vector_method_as_iterator},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_VectorIterator) {
    .superclass = &LT_Iterator_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "VectorIterator",
    .documentation = "Iterator over vector elements.",
    .instance_size = sizeof(LT_VectorIterator),
    .debugPrintOn = VectorIterator_debugPrintOn,
    .methods = VectorIterator_methods,
};

LT_DEFINE_CLASS(LT_Vector) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Vector",
    .documentation = "Mutable indexed sequence of object values.",
    .instance_size = sizeof(LT_Vector),
    .class_flags = LT_CLASS_FLAG_FLEXIBLE,
    .hash = Vector_hash,
    .equal_p = Vector_equal_p,
    .debugPrintOn = Vector_debugPrintOn,
    .methods = Vector_methods,
};

LT_Vector* LT_Vector_new(size_t length){
    LT_Vector* vector = LT_Class_ALLOC_FLEXIBLE(LT_Vector, sizeof(LT_Value) * length);
    size_t i;

    vector->length = length;
    for (i = 0; i < length; i++){
        vector->items[i] = LT_NIL;
    }
    return vector;
}

LT_Vector* LT_Vector_sortUsing(LT_Vector* vector, LT_Value callable){
    size_t length = LT_Vector_length(vector);
    LT_Vector* result = LT_Vector_new(length);
    size_t i;

    for (i = 0; i < length; i++){
        result->items[i] = vector->items[i];
    }
    sort_values(result->items, length, callable);
    return result;
}

LT_Vector* LT_Vector_sort(LT_Vector* vector){
    return LT_Vector_sortUsing(vector, LT_NIL);
}

size_t LT_Vector_length(LT_Vector* vector){
    return vector->length;
}

LT_Value LT_Vector_at(LT_Vector* vector, size_t index){
    if (index >= vector->length){
        LT_error("Vector index out of bounds");
    }
    return vector->items[index];
}

void LT_Vector_atPut(LT_Vector* vector, size_t index, LT_Value value){
    if (index >= vector->length){
        LT_error("Vector index out of bounds");
    }
    vector->items[index] = value;
}
