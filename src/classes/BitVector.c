/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/BitVector.h>
#include <ListTalk/classes/Boolean.h>
#include <ListTalk/classes/Class.h>
#include <ListTalk/classes/Iterator.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/error.h>

#include <stdint.h>
#include <string.h>

struct LT_BitVector_s {
    LT_Object base;
    size_t length;
    uint8_t bytes[];
};

struct LT_BitVectorIterator_s {
    LT_Object base;
    LT_BitVector* bitvector;
    size_t index;
};

static size_t byte_length_for_bits(size_t length){
    return length / 8 + (length % 8 != 0);
}

static int bit_from_value(LT_Value value){
    if (LT_Value_is_boolean(value)){
        return LT_Value_boolean_value(value);
    }
    if (LT_SmallInteger_p(value)){
        int64_t integer = LT_SmallInteger_value(value);

        if (integer == 0 || integer == 1){
            return (int)integer;
        }
    }
    LT_error("BitVector value must be 0, 1, #false, or #true");
}

static void BitVector_debugPrintOn(LT_Value value, FILE* stream){
    LT_BitVector* bitvector = LT_BitVector_from_value(value);
    size_t i;

    fputs("#<BitVector", stream);
    for (i = 0; i < bitvector->length; i++){
        fputc(' ', stream);
        fputs(LT_BitVector_at(bitvector, i) ? "#true" : "#false", stream);
    }
    fputc('>', stream);
}

static void BitVectorIterator_debugPrintOn(LT_Value value, FILE* stream){
    LT_BitVectorIterator* iterator = LT_BitVectorIterator_from_value(value);

    fprintf(
        stream,
        "#<BitVectorIterator %p index=%zu>",
        (void*)iterator,
        iterator->index
    );
}

static LT_Value BitVectorIterator_current(LT_BitVectorIterator* iterator){
    if (iterator->index >= LT_BitVector_length(iterator->bitvector)){
        LT_error("BitVectorIterator is not positioned");
    }
    return LT_BitVector_at(iterator->bitvector, iterator->index)
        ? LT_TRUE
        : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    bitvector_iterator_method_this,
    "BitVectorIterator>>this",
    "(self)",
    "Return the current bit as a Boolean."
){
    LT_Value cursor = arguments;
    LT_BitVectorIterator* iterator;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(
        cursor,
        iterator,
        LT_BitVectorIterator*,
        LT_BitVectorIterator_from_value
    );
    LT_ARG_END(cursor);
    return BitVectorIterator_current(iterator);
}

LT_DEFINE_PRIMITIVE(
    bitvector_iterator_method_has_this,
    "BitVectorIterator>>hasThis?",
    "(self)",
    "Return true when the iterator has a current bit."
){
    LT_Value cursor = arguments;
    LT_BitVectorIterator* iterator;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(
        cursor,
        iterator,
        LT_BitVectorIterator*,
        LT_BitVectorIterator_from_value
    );
    LT_ARG_END(cursor);
    return iterator->index < LT_BitVector_length(iterator->bitvector)
        ? LT_TRUE
        : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    bitvector_iterator_method_next,
    "BitVectorIterator>>next!",
    "(self)",
    "Advance the iterator and return receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_BitVectorIterator* iterator;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    iterator = LT_BitVectorIterator_from_value(self);
    if (iterator->index < LT_BitVector_length(iterator->bitvector)){
        iterator->index++;
    }
    return self;
}

LT_DEFINE_PRIMITIVE(
    bitvector_class_method_new,
    "BitVector class>>new:",
    "(self size)",
    "Return a new false-filled bit vector of the requested size."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value size;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, size);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_BitVector_class){
        LT_error("new: class method is only supported on BitVector");
    }
    return (LT_Value)(uintptr_t)LT_BitVector_new(
        LT_Number_nonnegative_size_from_integer(
            size,
            "BitVector size out of bounds",
            "BitVector size out of bounds"
        ),
        0
    );
}

LT_DEFINE_PRIMITIVE(
    bitvector_class_method_new_filled,
    "BitVector class>>new:filled:",
    "(self size fill)",
    "Return a new bit vector of the requested size and fill value."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value size;
    LT_Value fill;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, size);
    LT_OBJECT_ARG(cursor, fill);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_BitVector_class){
        LT_error("new:filled: class method is only supported on BitVector");
    }
    return (LT_Value)(uintptr_t)LT_BitVector_new(
        LT_Number_nonnegative_size_from_integer(
            size,
            "BitVector size out of bounds",
            "BitVector size out of bounds"
        ),
        bit_from_value(fill)
    );
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_at,
    "BitVector>>at:",
    "(self index)",
    "Return the bit at index as a Boolean."
){
    LT_Value cursor = arguments;
    LT_BitVector* bitvector;
    LT_Value index;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, bitvector, LT_BitVector*, LT_BitVector_from_value);
    LT_OBJECT_ARG(cursor, index);
    LT_ARG_END(cursor);
    return LT_BitVector_at(
        bitvector,
        LT_Number_nonnegative_size_from_integer(
            index,
            "BitVector index out of bounds",
            "BitVector index out of bounds"
        )
    ) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_at_put,
    "BitVector>>at:put:",
    "(self index value)",
    "Set the bit at index and return its Boolean value."
){
    LT_Value cursor = arguments;
    LT_BitVector* bitvector;
    LT_Value index;
    LT_Value value;
    int bit;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, bitvector, LT_BitVector*, LT_BitVector_from_value);
    LT_OBJECT_ARG(cursor, index);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    bit = bit_from_value(value);
    LT_BitVector_atPut(
        bitvector,
        LT_Number_nonnegative_size_from_integer(
            index,
            "BitVector index out of bounds",
            "BitVector index out of bounds"
        ),
        bit
    );
    return bit ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_as_iterator,
    "BitVector>>asIterator",
    "(self)",
    "Return an iterator over the bits as Booleans."
){
    LT_Value cursor = arguments;
    LT_BitVector* bitvector;
    LT_BitVectorIterator* iterator;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, bitvector, LT_BitVector*, LT_BitVector_from_value);
    LT_ARG_END(cursor);
    if (LT_BitVector_length(bitvector) == 0){
        return (LT_Value)(uintptr_t)LT_EmptyIterator_instance();
    }

    iterator = LT_Class_ALLOC(LT_BitVectorIterator);
    iterator->bitvector = bitvector;
    iterator->index = 0;
    return (LT_Value)(uintptr_t)iterator;
}

static LT_Method_Descriptor BitVectorIterator_methods[] = {
    {"this", &bitvector_iterator_method_this},
    {"hasThis?", &bitvector_iterator_method_has_this},
    {"next!", &bitvector_iterator_method_next},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor BitVector_methods[] = {
    {"at:", &bitvector_method_at},
    {"at:put:", &bitvector_method_at_put},
    {"asIterator", &bitvector_method_as_iterator},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor BitVector_class_methods[] = {
    {"new:", &bitvector_class_method_new},
    {"new:filled:", &bitvector_class_method_new_filled},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_BitVectorIterator) {
    .superclass = &LT_Iterator_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "BitVectorIterator",
    .documentation = "Iterator over bit vector elements.",
    .instance_size = sizeof(LT_BitVectorIterator),
    .debugPrintOn = BitVectorIterator_debugPrintOn,
    .methods = BitVectorIterator_methods,
};

LT_DEFINE_CLASS(LT_BitVector) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "BitVector",
    .documentation = "Mutable indexed sequence of bits.",
    .instance_size = sizeof(LT_BitVector),
    .class_flags = LT_CLASS_FLAG_FLEXIBLE,
    .debugPrintOn = BitVector_debugPrintOn,
    .methods = BitVector_methods,
    .class_methods = BitVector_class_methods,
};

LT_BitVector* LT_BitVector_new(size_t length, int fill){
    size_t byte_length = byte_length_for_bits(length);
    LT_BitVector* bitvector = LT_Class_ALLOC_FLEXIBLE(
        LT_BitVector,
        byte_length
    );

    bitvector->length = length;
    memset(bitvector->bytes, fill ? UINT8_MAX : 0, byte_length);
    return bitvector;
}

size_t LT_BitVector_length(LT_BitVector* bitvector){
    return bitvector->length;
}

int LT_BitVector_at(LT_BitVector* bitvector, size_t index){
    if (index >= bitvector->length){
        LT_error("BitVector index out of bounds");
    }
    return (bitvector->bytes[index / 8] >> (index % 8)) & 1;
}

void LT_BitVector_atPut(LT_BitVector* bitvector, size_t index, int value){
    uint8_t mask;

    if (index >= bitvector->length){
        LT_error("BitVector index out of bounds");
    }
    mask = (uint8_t)(UINT8_C(1) << (index % 8));
    if (value){
        bitvector->bytes[index / 8] |= mask;
    } else {
        bitvector->bytes[index / 8] &= (uint8_t)~mask;
    }
}
