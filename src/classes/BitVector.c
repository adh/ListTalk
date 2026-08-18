/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/BitVector.h>
#include <ListTalk/classes/Boolean.h>
#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/Class.h>
#include <ListTalk/classes/Iterator.h>
#include <ListTalk/classes/Integer.h>
#include <ListTalk/classes/List.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/Vector.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/utils.h>

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

static void require_same_length(LT_BitVector* left, LT_BitVector* right){
    if (left->length != right->length){
        LT_error("BitVector lengths must match");
    }
}

static void require_span(LT_BitVector* bitvector, size_t at, size_t length){
    if (at > bitvector->length || length > bitvector->length - at){
        LT_error("BitVector range out of bounds");
    }
}

static LT_BitVector* bitvector_binary(LT_BitVector* left,
                                      LT_BitVector* right,
                                      int operation){
    LT_BitVector* result;
    size_t i;

    require_same_length(left, right);
    result = LT_BitVector_new(left->length, 0);
    for (i = 0; i < left->length; i++){
        int a = LT_BitVector_at(left, i);
        int b = LT_BitVector_at(right, i);
        int value = operation == 0 ? (a & b)
            : operation == 1 ? (a | b)
            : (a ^ b);

        LT_BitVector_atPut(result, i, value);
    }
    return result;
}

static size_t BitVector_hash(LT_Value value){
    LT_BitVector* bitvector = LT_BitVector_from_value(value);
    size_t hash = (size_t)0x9e3779b1 ^ bitvector->length;
    size_t i;

    for (i = 0; i < bitvector->length; i++){
        hash = (hash * (size_t)33) ^ (size_t)LT_BitVector_at(bitvector, i);
    }
    return hash;
}

static int BitVector_equal_p(LT_Value left, LT_Value right){
    LT_BitVector* a;
    LT_BitVector* b;
    size_t i;

    if (!LT_BitVector_p(right)){
        return 0;
    }
    a = LT_BitVector_from_value(left);
    b = LT_BitVector_from_value(right);
    if (a->length != b->length){
        return 0;
    }
    for (i = 0; i < a->length; i++){
        if (LT_BitVector_at(a, i) != LT_BitVector_at(b, i)){
            return 0;
        }
    }
    return 1;
}

static void require_bitvector_class(LT_Value self, const char* selector){
    (void)selector;
    if (self != (LT_Value)(uintptr_t)&LT_BitVector_class){
        LT_error("Class method is only supported on BitVector");
    }
}

static LT_BitVector* bitvector_from_be_bytes(LT_ByteVector* bytes,
                                             size_t bit_length){
    LT_BitVector* bitvector = LT_BitVector_new(bit_length, 0);
    size_t byte_length = LT_ByteVector_length(bytes);
    size_t i;

    for (i = 0; i < byte_length; i++){
        uint8_t byte = LT_ByteVector_at(bytes, byte_length - i - 1);
        size_t destination = i * 8;
        size_t remaining;

        if (destination >= bit_length){
            break;
        }
        remaining = bit_length - destination;
        if (remaining < 8){
            byte &= (uint8_t)(UINT8_C(1) << remaining) - 1;
        }
        bitvector->bytes[i] = byte;
    }
    return bitvector;
}

static LT_BitVector* bitvector_from_le_bytes(LT_ByteVector* bytes){
    size_t byte_length = LT_ByteVector_length(bytes);
    LT_BitVector* bitvector;
    size_t i;

    if (byte_length > SIZE_MAX / 8){
        LT_error("ByteVector is too large for a BitVector");
    }
    bitvector = LT_BitVector_new(byte_length * 8, 0);
    for (i = 0; i < byte_length; i++){
        bitvector->bytes[i] = LT_ByteVector_at(bytes, i);
    }
    return bitvector;
}

static LT_ByteVector* bitvector_as_le_bytes(LT_BitVector* bitvector){
    size_t byte_length = byte_length_for_bits(bitvector->length);
    LT_ByteVector* bytes = LT_ByteVector_new_filled(byte_length, 0);
    size_t i;

    for (i = 0; i < byte_length; i++){
        uint8_t byte = bitvector->bytes[i];

        if (i + 1 == byte_length && bitvector->length % 8 != 0){
            byte &= (uint8_t)(UINT8_C(1) << (bitvector->length % 8)) - 1;
        }
        LT_ByteVector_atPut(bytes, i, byte);
    }
    return bytes;
}

static LT_ByteVector* bitvector_as_be_bytes(LT_BitVector* bitvector){
    LT_ByteVector* little_endian = bitvector_as_le_bytes(bitvector);
    size_t length = LT_ByteVector_length(little_endian);
    LT_ByteVector* big_endian = LT_ByteVector_new_filled(length, 0);
    size_t i;

    for (i = 0; i < length; i++){
        LT_ByteVector_atPut(
            big_endian,
            i,
            LT_ByteVector_at(little_endian, length - i - 1)
        );
    }
    return big_endian;
}

static size_t unsigned_bit_length(LT_ByteVector* bytes){
    size_t byte_length = LT_ByteVector_length(bytes);
    uint8_t first;
    size_t high_bits = 0;

    if (byte_length == 0){
        return 1;
    }
    first = LT_ByteVector_at(bytes, 0);
    while (first != 0){
        first >>= 1;
        high_bits++;
    }
    if (high_bits == 0){
        return 1;
    }
    return (byte_length - 1) * 8 + high_bits;
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
    bitvector_class_method_from_unsigned_integer,
    "BitVector class>>fromUnsignedInteger:",
    "(self integer)",
    "Return the shortest bit vector representing a non-negative integer."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value integer;
    LT_ByteVector* bytes;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, integer);
    LT_ARG_END(cursor);
    require_bitvector_class(self, "fromUnsignedInteger:");
    bytes = LT_ByteVector_from_value(LT_SEND(integer, "toBytes"));
    return (LT_Value)(uintptr_t)bitvector_from_be_bytes(
        bytes,
        unsigned_bit_length(bytes)
    );
}

LT_DEFINE_PRIMITIVE(
    bitvector_class_method_from_unsigned_integer_size,
    "BitVector class>>fromUnsignedInteger:size:",
    "(self integer size)",
    "Return an exact-size bit vector representing a non-negative integer."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value integer;
    LT_Value size_value;
    size_t size;
    size_t byte_length;
    LT_ByteVector* bytes;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, integer);
    LT_OBJECT_ARG(cursor, size_value);
    LT_ARG_END(cursor);
    require_bitvector_class(self, "fromUnsignedInteger:size:");
    size = LT_Number_nonnegative_size_from_integer(
        size_value,
        "BitVector size out of bounds",
        "BitVector size out of bounds"
    );
    byte_length = byte_length_for_bits(size);
    bytes = LT_ByteVector_from_value(LT_SEND(
        integer,
        "toBytes:",
        LT_Number_smallinteger_from_size(
            byte_length,
            "BitVector size out of bounds"
        )
    ));
    if (size % 8 != 0
        && (LT_ByteVector_at(bytes, 0) >> (size % 8)) != 0){
        LT_error("Unsigned integer does not fit requested BitVector size");
    }
    return (LT_Value)(uintptr_t)bitvector_from_be_bytes(bytes, size);
}

LT_DEFINE_PRIMITIVE(
    bitvector_class_method_from_integer_size,
    "BitVector class>>fromInteger:size:",
    "(self integer size)",
    "Return an exact-size two's-complement bit vector."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value integer;
    LT_Value size_value;
    size_t size;
    size_t byte_length;
    LT_ByteVector* bytes;
    uint8_t high_byte;
    uint8_t value_mask;
    uint8_t extension_mask;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, integer);
    LT_OBJECT_ARG(cursor, size_value);
    LT_ARG_END(cursor);
    require_bitvector_class(self, "fromInteger:size:");
    size = LT_Number_nonnegative_size_from_integer(
        size_value,
        "BitVector size out of bounds",
        "BitVector size out of bounds"
    );
    if (size == 0){
        LT_error("Signed BitVector size must be positive");
    }
    byte_length = byte_length_for_bits(size);
    bytes = LT_ByteVector_from_value(LT_SEND(
        integer,
        "toTwosComplement:",
        LT_Number_smallinteger_from_size(
            byte_length,
            "BitVector size out of bounds"
        )
    ));
    high_byte = LT_ByteVector_at(bytes, 0);
    if (size % 8 != 0){
        value_mask = (uint8_t)(UINT8_C(1) << (size % 8)) - 1;
        extension_mask = (uint8_t)~value_mask;
        if ((high_byte & (UINT8_C(1) << ((size - 1) % 8))) != 0){
            if ((high_byte & extension_mask) != extension_mask){
                LT_error("Integer does not fit requested BitVector size");
            }
        } else if ((high_byte & extension_mask) != 0){
            LT_error("Integer does not fit requested BitVector size");
        }
    }
    return (LT_Value)(uintptr_t)bitvector_from_be_bytes(bytes, size);
}

LT_DEFINE_PRIMITIVE(
    bitvector_class_method_from_list,
    "BitVector class>>fromList:",
    "(self list)",
    "Return a bit vector whose index order matches the list."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value list;
    LT_Value rest;
    size_t length = 0;
    size_t index = 0;
    LT_BitVector* bitvector;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, list);
    LT_ARG_END(cursor);
    require_bitvector_class(self, "fromList:");
    if (!LT_List_p(list) || !LT_List_proper_p(list)){
        LT_error("BitVector class>>fromList: requires a proper List");
    }
    rest = list;
    while (rest != LT_NIL){
        if (length == SIZE_MAX){
            LT_error("List is too large for a BitVector");
        }
        length++;
        rest = LT_cdr(rest);
    }
    bitvector = LT_BitVector_new(length, 0);
    rest = list;
    while (rest != LT_NIL){
        LT_BitVector_atPut(bitvector, index++, bit_from_value(LT_car(rest)));
        rest = LT_cdr(rest);
    }
    return (LT_Value)(uintptr_t)bitvector;
}

LT_DEFINE_PRIMITIVE(
    bitvector_class_method_from_le_bytevector,
    "BitVector class>>fromLEByteVector:",
    "(self bytevector)",
    "Return a bit vector decoded from little-endian bytes."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_ByteVector* bytes;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, bytes, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    require_bitvector_class(self, "fromLEByteVector:");
    return (LT_Value)(uintptr_t)bitvector_from_le_bytes(bytes);
}

LT_DEFINE_PRIMITIVE(
    bitvector_class_method_from_be_bytevector,
    "BitVector class>>fromBEByteVector:",
    "(self bytevector)",
    "Return a bit vector decoded from big-endian bytes."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_ByteVector* bytes;
    size_t byte_length;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, bytes, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    require_bitvector_class(self, "fromBEByteVector:");
    byte_length = LT_ByteVector_length(bytes);
    if (byte_length > SIZE_MAX / 8){
        LT_error("ByteVector is too large for a BitVector");
    }
    return (LT_Value)(uintptr_t)bitvector_from_be_bytes(
        bytes,
        byte_length * 8
    );
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
    "Put a bit or all bits of a BitVector starting at index and return value."
){
    LT_Value cursor = arguments;
    LT_BitVector* bitvector;
    LT_Value index;
    LT_Value value;
    size_t at;
    size_t i;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, bitvector, LT_BitVector*, LT_BitVector_from_value);
    LT_OBJECT_ARG(cursor, index);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    at = LT_Number_nonnegative_size_from_integer(
        index,
        "BitVector index out of bounds",
        "BitVector index out of bounds"
    );
    if (LT_BitVector_p(value)){
        LT_BitVector* source = LT_BitVector_from_value(value);

        require_span(bitvector, at, source->length);
        for (i = 0; i < source->length; i++){
            LT_BitVector_atPut(bitvector, at + i, LT_BitVector_at(source, i));
        }
        return value;
    }
    LT_BitVector_atPut(bitvector, at, bit_from_value(value));
    return LT_BitVector_at(bitvector, at) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_pop_count,
    "BitVector>>popCount",
    "(self)",
    "Return the number of set bits."
){
    LT_Value cursor = arguments;
    LT_BitVector* bitvector;
    size_t count = 0;
    size_t i;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, bitvector, LT_BitVector*, LT_BitVector_from_value);
    LT_ARG_END(cursor);
    for (i = 0; i < bitvector->length; i++){
        count += (size_t)LT_BitVector_at(bitvector, i);
    }
    return LT_Number_smallinteger_from_size(
        count,
        "BitVector population count does not fit fixnum"
    );
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_at_length,
    "BitVector>>at:length:",
    "(self index length)",
    "Return a copy of a range of bits."
){
    LT_Value cursor = arguments;
    LT_BitVector* bitvector;
    LT_Value index_value;
    LT_Value length_value;
    LT_BitVector* result;
    size_t index;
    size_t length;
    size_t i;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, bitvector, LT_BitVector*, LT_BitVector_from_value);
    LT_OBJECT_ARG(cursor, index_value);
    LT_OBJECT_ARG(cursor, length_value);
    LT_ARG_END(cursor);
    index = LT_Number_nonnegative_size_from_integer(
        index_value, "BitVector range out of bounds", "BitVector range out of bounds"
    );
    length = LT_Number_nonnegative_size_from_integer(
        length_value, "BitVector range out of bounds", "BitVector range out of bounds"
    );
    require_span(bitvector, index, length);
    result = LT_BitVector_new(length, 0);
    for (i = 0; i < length; i++){
        LT_BitVector_atPut(result, i, LT_BitVector_at(bitvector, index + i));
    }
    return (LT_Value)(uintptr_t)result;
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_combine_with_using,
    "BitVector>>combineWith:using:",
    "(self other combination)",
    "Combine BitVectors using a four-entry table, 4-bit integer, or function."
){
    LT_Value cursor = arguments;
    LT_BitVector* left;
    LT_BitVector* right;
    LT_Value combination;
    LT_BitVector* result;
    int64_t integer_table = 0;
    int combination_is_integer;
    size_t i;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, left, LT_BitVector*, LT_BitVector_from_value);
    LT_GENERIC_ARG(cursor, right, LT_BitVector*, LT_BitVector_from_value);
    LT_OBJECT_ARG(cursor, combination);
    LT_ARG_END(cursor);
    require_same_length(left, right);
    if (LT_BitVector_p(combination)
        && LT_BitVector_length(LT_BitVector_from_value(combination)) != 4){
        LT_error("BitVector combination table must have four elements");
    }
    if (LT_Vector_p(combination)
        && LT_Vector_length(LT_Vector_from_value(combination)) != 4){
        LT_error("BitVector combination table must have four elements");
    }
    combination_is_integer = LT_Value_is_instance_of(
        combination,
        LT_STATIC_CLASS(LT_Integer)
    );
    if (combination_is_integer){
        if (!LT_SmallInteger_p(combination)
            || (integer_table = LT_SmallInteger_value(combination)) < 0
            || integer_table > 15){
            LT_error("BitVector integer combination table must be between 0 and 15");
        }
    }
    result = LT_BitVector_new(left->length, 0);
    for (i = 0; i < left->length; i++){
        int a = LT_BitVector_at(left, i);
        int b = LT_BitVector_at(right, i);
        size_t table_index = (size_t)(a * 2 + b);
        LT_Value value;

        if (LT_BitVector_p(combination)){
            value = LT_BitVector_at(LT_BitVector_from_value(combination), table_index)
                ? LT_TRUE : LT_FALSE;
        } else if (LT_Vector_p(combination)){
            value = LT_Vector_at(LT_Vector_from_value(combination), table_index);
        } else if (combination_is_integer){
            value = (integer_table >> table_index) & 1 ? LT_TRUE : LT_FALSE;
        } else {
            value = LT_APPLY(
                combination,
                a ? LT_TRUE : LT_FALSE,
                b ? LT_TRUE : LT_FALSE
            );
        }
        LT_BitVector_atPut(result, i, bit_from_value(value));
    }
    return (LT_Value)(uintptr_t)result;
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_and,
    "BitVector>>and:",
    "(self other)",
    "Return the bitwise conjunction with another BitVector."
){
    LT_Value cursor = arguments;
    LT_BitVector* left;
    LT_BitVector* right;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, left, LT_BitVector*, LT_BitVector_from_value);
    LT_GENERIC_ARG(cursor, right, LT_BitVector*, LT_BitVector_from_value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)bitvector_binary(left, right, 0);
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_or,
    "BitVector>>or:",
    "(self other)",
    "Return the bitwise disjunction with another BitVector."
){
    LT_Value cursor = arguments;
    LT_BitVector* left;
    LT_BitVector* right;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, left, LT_BitVector*, LT_BitVector_from_value);
    LT_GENERIC_ARG(cursor, right, LT_BitVector*, LT_BitVector_from_value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)bitvector_binary(left, right, 1);
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_xor,
    "BitVector>>xor:",
    "(self other)",
    "Return the bitwise exclusive disjunction with another BitVector."
){
    LT_Value cursor = arguments;
    LT_BitVector* left;
    LT_BitVector* right;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, left, LT_BitVector*, LT_BitVector_from_value);
    LT_GENERIC_ARG(cursor, right, LT_BitVector*, LT_BitVector_from_value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)bitvector_binary(left, right, 2);
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_not,
    "BitVector>>not",
    "(self)",
    "Return a BitVector with every bit inverted."
){
    LT_Value cursor = arguments;
    LT_BitVector* bitvector;
    LT_BitVector* result;
    size_t i;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, bitvector, LT_BitVector*, LT_BitVector_from_value);
    LT_ARG_END(cursor);
    result = LT_BitVector_new(bitvector->length, 0);
    for (i = 0; i < bitvector->length; i++){
        LT_BitVector_atPut(result, i, !LT_BitVector_at(bitvector, i));
    }
    return (LT_Value)(uintptr_t)result;
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_choose_or,
    "BitVector>>choose:or:",
    "(self whenTrue whenFalse)",
    "Select bits from the first or second BitVector according to receiver."
){
    LT_Value cursor = arguments;
    LT_BitVector* selector;
    LT_BitVector* when_true;
    LT_BitVector* when_false;
    LT_BitVector* result;
    size_t i;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, selector, LT_BitVector*, LT_BitVector_from_value);
    LT_GENERIC_ARG(cursor, when_true, LT_BitVector*, LT_BitVector_from_value);
    LT_GENERIC_ARG(cursor, when_false, LT_BitVector*, LT_BitVector_from_value);
    LT_ARG_END(cursor);
    require_same_length(selector, when_true);
    require_same_length(selector, when_false);
    result = LT_BitVector_new(selector->length, 0);
    for (i = 0; i < selector->length; i++){
        LT_BitVector_atPut(
            result,
            i,
            LT_BitVector_at(selector, i)
                ? LT_BitVector_at(when_true, i)
                : LT_BitVector_at(when_false, i)
        );
    }
    return (LT_Value)(uintptr_t)result;
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_length,
    "BitVector>>length",
    "(self)",
    "Return the number of bits."
){
    LT_Value cursor = arguments;
    LT_BitVector* bitvector;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, bitvector, LT_BitVector*, LT_BitVector_from_value);
    LT_ARG_END(cursor);
    return LT_Number_smallinteger_from_size(
        bitvector->length,
        "BitVector length does not fit fixnum"
    );
}

static LT_BitVector* bitvector_shift(LT_BitVector* source,
                                     size_t amount,
                                     int left){
    LT_BitVector* result = LT_BitVector_new(source->length, 0);
    size_t i;

    if (amount >= source->length){
        return result;
    }
    if (left){
        for (i = amount; i < source->length; i++){
            LT_BitVector_atPut(result, i, LT_BitVector_at(source, i - amount));
        }
    } else {
        for (i = 0; i < source->length - amount; i++){
            LT_BitVector_atPut(result, i, LT_BitVector_at(source, i + amount));
        }
    }
    return result;
}

#define DEFINE_BITVECTOR_SHIFT_METHOD(c_name, selector, direction) \
LT_DEFINE_PRIMITIVE( \
    c_name, selector, "(self amount)", "Return a logically shifted BitVector." \
){ \
    LT_Value cursor = arguments; \
    LT_BitVector* bitvector; \
    LT_Value amount_value; \
    size_t amount; \
    (void)tail_call_unwind_marker; \
    LT_GENERIC_ARG(cursor, bitvector, LT_BitVector*, LT_BitVector_from_value); \
    LT_OBJECT_ARG(cursor, amount_value); \
    LT_ARG_END(cursor); \
    amount = LT_Number_nonnegative_size_from_integer( \
        amount_value, "BitVector shift must be nonnegative", \
        "BitVector shift out of range" \
    ); \
    return (LT_Value)(uintptr_t)bitvector_shift(bitvector, amount, direction); \
}

DEFINE_BITVECTOR_SHIFT_METHOD(
    bitvector_method_shift_left, "BitVector>>shiftLeft:", 1
)
DEFINE_BITVECTOR_SHIFT_METHOD(
    bitvector_method_shift_right, "BitVector>>shiftRight:", 0
)

static LT_Value bitvector_rotate_primitive(LT_Value arguments, int left){
    LT_Value cursor = arguments;
    LT_BitVector* bitvector;
    LT_Value amount_value;
    LT_BitVector* result;
    size_t amount;
    size_t i;

    LT_GENERIC_ARG(cursor, bitvector, LT_BitVector*, LT_BitVector_from_value);
    LT_OBJECT_ARG(cursor, amount_value);
    LT_ARG_END(cursor);
    amount = LT_Number_nonnegative_size_from_integer(
        amount_value,
        "BitVector rotation must be nonnegative",
        "BitVector rotation out of range"
    );
    result = LT_BitVector_new(bitvector->length, 0);
    if (bitvector->length == 0){
        return (LT_Value)(uintptr_t)result;
    }
    amount %= bitvector->length;
    for (i = 0; i < bitvector->length; i++){
        size_t destination = left
            ? (i + amount) % bitvector->length
            : (i + bitvector->length - amount) % bitvector->length;
        LT_BitVector_atPut(result, destination, LT_BitVector_at(bitvector, i));
    }
    return (LT_Value)(uintptr_t)result;
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_rotate_left,
    "BitVector>>rotateLeft:",
    "(self amount)",
    "Return a left-rotated BitVector."
){
    (void)tail_call_unwind_marker;
    return bitvector_rotate_primitive(arguments, 1);
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_rotate_right,
    "BitVector>>rotateRight:",
    "(self amount)",
    "Return a right-rotated BitVector."
){
    (void)tail_call_unwind_marker;
    return bitvector_rotate_primitive(arguments, 0);
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_append,
    "BitVector>>append:",
    "(self other)",
    "Return the concatenation of receiver and another BitVector."
){
    LT_Value cursor = arguments;
    LT_BitVector* left;
    LT_BitVector* right;
    LT_BitVector* result;
    size_t i;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, left, LT_BitVector*, LT_BitVector_from_value);
    LT_GENERIC_ARG(cursor, right, LT_BitVector*, LT_BitVector_from_value);
    LT_ARG_END(cursor);
    if (right->length > SIZE_MAX - left->length){
        LT_error("Concatenated BitVector is too large");
    }
    result = LT_BitVector_new(left->length + right->length, 0);
    for (i = 0; i < left->length; i++){
        LT_BitVector_atPut(result, i, LT_BitVector_at(left, i));
    }
    for (i = 0; i < right->length; i++){
        LT_BitVector_atPut(result, left->length + i, LT_BitVector_at(right, i));
    }
    return (LT_Value)(uintptr_t)result;
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_resize_filled,
    "BitVector>>resize:filled:",
    "(self length fill)",
    "Return a resized copy, filling newly added bits with fill."
){
    LT_Value cursor = arguments;
    LT_BitVector* bitvector;
    LT_Value length_value;
    LT_Value fill_value;
    LT_BitVector* result;
    size_t length;
    size_t copied;
    size_t i;
    int fill;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, bitvector, LT_BitVector*, LT_BitVector_from_value);
    LT_OBJECT_ARG(cursor, length_value);
    LT_OBJECT_ARG(cursor, fill_value);
    LT_ARG_END(cursor);
    length = LT_Number_nonnegative_size_from_integer(
        length_value, "BitVector size out of bounds", "BitVector size out of bounds"
    );
    fill = bit_from_value(fill_value);
    result = LT_BitVector_new(length, fill);
    copied = length < bitvector->length ? length : bitvector->length;
    for (i = 0; i < copied; i++){
        LT_BitVector_atPut(result, i, LT_BitVector_at(bitvector, i));
    }
    return (LT_Value)(uintptr_t)result;
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_rank,
    "BitVector>>rank:",
    "(self index)",
    "Return the number of set bits before index."
){
    LT_Value cursor = arguments;
    LT_BitVector* bitvector;
    LT_Value index_value;
    size_t index;
    size_t count = 0;
    size_t i;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, bitvector, LT_BitVector*, LT_BitVector_from_value);
    LT_OBJECT_ARG(cursor, index_value);
    LT_ARG_END(cursor);
    index = LT_Number_nonnegative_size_from_integer(
        index_value, "BitVector rank index out of bounds",
        "BitVector rank index out of bounds"
    );
    if (index > bitvector->length){
        LT_error("BitVector rank index out of bounds");
    }
    for (i = 0; i < index; i++){
        count += (size_t)LT_BitVector_at(bitvector, i);
    }
    return LT_Number_smallinteger_from_size(count, "BitVector rank does not fit fixnum");
}

static LT_Value bitvector_find_set_bit(LT_BitVector* bitvector,
                                       size_t start,
                                       size_t ordinal,
                                       int use_ordinal){
    size_t i;
    size_t seen = 0;

    for (i = start; i < bitvector->length; i++){
        if (LT_BitVector_at(bitvector, i)){
            if (!use_ordinal || seen == ordinal){
                return LT_Number_smallinteger_from_size(
                    i, "BitVector index does not fit fixnum"
                );
            }
            seen++;
        }
    }
    return LT_NIL;
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_select,
    "BitVector>>select:",
    "(self ordinal)",
    "Return the index of the zero-based ordinal set bit, or nil."
){
    LT_Value cursor = arguments;
    LT_BitVector* bitvector;
    LT_Value ordinal_value;
    size_t ordinal;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, bitvector, LT_BitVector*, LT_BitVector_from_value);
    LT_OBJECT_ARG(cursor, ordinal_value);
    LT_ARG_END(cursor);
    ordinal = LT_Number_nonnegative_size_from_integer(
        ordinal_value, "BitVector select ordinal out of bounds",
        "BitVector select ordinal out of bounds"
    );
    return bitvector_find_set_bit(bitvector, 0, ordinal, 1);
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_first_set_bit,
    "BitVector>>firstSetBit",
    "(self)",
    "Return the first set-bit index, or nil."
){
    LT_Value cursor = arguments;
    LT_BitVector* bitvector;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, bitvector, LT_BitVector*, LT_BitVector_from_value);
    LT_ARG_END(cursor);
    return bitvector_find_set_bit(bitvector, 0, 0, 0);
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_next_set_bit_after,
    "BitVector>>nextSetBitAfter:",
    "(self index)",
    "Return the next set-bit index strictly after index, or nil."
){
    LT_Value cursor = arguments;
    LT_BitVector* bitvector;
    LT_Value index_value;
    size_t index;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, bitvector, LT_BitVector*, LT_BitVector_from_value);
    LT_OBJECT_ARG(cursor, index_value);
    LT_ARG_END(cursor);
    index = LT_Number_nonnegative_size_from_integer(
        index_value, "BitVector index out of bounds", "BitVector index out of bounds"
    );
    if (index >= bitvector->length){
        LT_error("BitVector index out of bounds");
    }
    return bitvector_find_set_bit(bitvector, index + 1, 0, 0);
}

static LT_Value bitvector_predicate(LT_BitVector* bitvector, int kind){
    size_t count = 0;
    size_t i;
    for (i = 0; i < bitvector->length; i++){
        count += (size_t)LT_BitVector_at(bitvector, i);
    }
    if (kind == 0){
        return count != 0 ? LT_TRUE : LT_FALSE;
    }
    if (kind == 1){
        return count == bitvector->length ? LT_TRUE : LT_FALSE;
    }
    return count == 0 ? LT_TRUE : LT_FALSE;
}

#define DEFINE_BITVECTOR_PREDICATE(c_name, selector, kind_value) \
LT_DEFINE_PRIMITIVE(c_name, selector, "(self)", "Query the receiver bits."){ \
    LT_Value cursor = arguments; \
    LT_BitVector* bitvector; \
    (void)tail_call_unwind_marker; \
    LT_GENERIC_ARG(cursor, bitvector, LT_BitVector*, LT_BitVector_from_value); \
    LT_ARG_END(cursor); \
    return bitvector_predicate(bitvector, kind_value); \
}

DEFINE_BITVECTOR_PREDICATE(bitvector_method_any, "BitVector>>any?", 0)
DEFINE_BITVECTOR_PREDICATE(bitvector_method_all, "BitVector>>all?", 1)
DEFINE_BITVECTOR_PREDICATE(bitvector_method_none, "BitVector>>none?", 2)

LT_DEFINE_PRIMITIVE(
    bitvector_method_reverse,
    "BitVector>>reverse",
    "(self)",
    "Return a BitVector with reversed index order."
){
    LT_Value cursor = arguments;
    LT_BitVector* bitvector;
    LT_BitVector* result;
    size_t i;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, bitvector, LT_BitVector*, LT_BitVector_from_value);
    LT_ARG_END(cursor);
    result = LT_BitVector_new(bitvector->length, 0);
    for (i = 0; i < bitvector->length; i++){
        LT_BitVector_atPut(
            result, bitvector->length - i - 1, LT_BitVector_at(bitvector, i)
        );
    }
    return (LT_Value)(uintptr_t)result;
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_as_unsigned_integer,
    "BitVector>>asUnsignedInteger",
    "(self)",
    "Return the bits interpreted as an unsigned integer."
){
    LT_Value cursor = arguments;
    LT_BitVector* bitvector;
    LT_ByteVector* bytes;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, bitvector, LT_BitVector*, LT_BitVector_from_value);
    LT_ARG_END(cursor);
    bytes = bitvector_as_be_bytes(bitvector);
    return LT_SEND(
        (LT_Value)(uintptr_t)&LT_Integer_class,
        "fromBytes:",
        (LT_Value)(uintptr_t)bytes
    );
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_as_signed_integer,
    "BitVector>>asSignedInteger",
    "(self)",
    "Return the bits interpreted as a two's-complement integer."
){
    LT_Value cursor = arguments;
    LT_BitVector* bitvector;
    LT_ByteVector* bytes;
    size_t remainder;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, bitvector, LT_BitVector*, LT_BitVector_from_value);
    LT_ARG_END(cursor);
    if (bitvector->length == 0){
        return LT_SmallInteger_new(0);
    }
    bytes = bitvector_as_be_bytes(bitvector);
    remainder = bitvector->length % 8;
    if (remainder != 0
        && LT_BitVector_at(bitvector, bitvector->length - 1)){
        uint8_t high_byte = LT_ByteVector_at(bytes, 0);
        uint8_t value_mask = (uint8_t)(UINT8_C(1) << remainder) - 1;

        LT_ByteVector_atPut(bytes, 0, high_byte | (uint8_t)~value_mask);
    }
    return LT_SEND(
        (LT_Value)(uintptr_t)&LT_Integer_class,
        "fromTwosComplement:",
        (LT_Value)(uintptr_t)bytes
    );
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_as_list,
    "BitVector>>asList",
    "(self)",
    "Return the bits as a list of Booleans in index order."
){
    LT_Value cursor = arguments;
    LT_BitVector* bitvector;
    LT_ListBuilder* builder;
    size_t i;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, bitvector, LT_BitVector*, LT_BitVector_from_value);
    LT_ARG_END(cursor);
    builder = LT_ListBuilder_new();
    for (i = 0; i < bitvector->length; i++){
        LT_ListBuilder_append(
            builder,
            LT_BitVector_at(bitvector, i) ? LT_TRUE : LT_FALSE
        );
    }
    return LT_ListBuilder_value(builder);
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_as_le_bytevector,
    "BitVector>>asLEByteVector",
    "(self)",
    "Return the bits packed into little-endian bytes."
){
    LT_Value cursor = arguments;
    LT_BitVector* bitvector;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, bitvector, LT_BitVector*, LT_BitVector_from_value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)bitvector_as_le_bytes(bitvector);
}

LT_DEFINE_PRIMITIVE(
    bitvector_method_as_be_bytevector,
    "BitVector>>asBEByteVector",
    "(self)",
    "Return the bits packed into big-endian bytes."
){
    LT_Value cursor = arguments;
    LT_BitVector* bitvector;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, bitvector, LT_BitVector*, LT_BitVector_from_value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)bitvector_as_be_bytes(bitvector);
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
    {"length", &bitvector_method_length},
    {"at:", &bitvector_method_at},
    {"at:length:", &bitvector_method_at_length},
    {"at:put:", &bitvector_method_at_put},
    {"popCount", &bitvector_method_pop_count},
    {"choose:or:", &bitvector_method_choose_or},
    {"and:", &bitvector_method_and},
    {"or:", &bitvector_method_or},
    {"xor:", &bitvector_method_xor},
    {"not", &bitvector_method_not},
    {"combineWith:using:", &bitvector_method_combine_with_using},
    {"shiftLeft:", &bitvector_method_shift_left},
    {"shiftRight:", &bitvector_method_shift_right},
    {"rotateLeft:", &bitvector_method_rotate_left},
    {"rotateRight:", &bitvector_method_rotate_right},
    {"append:", &bitvector_method_append},
    {"resize:filled:", &bitvector_method_resize_filled},
    {"rank:", &bitvector_method_rank},
    {"select:", &bitvector_method_select},
    {"firstSetBit", &bitvector_method_first_set_bit},
    {"nextSetBitAfter:", &bitvector_method_next_set_bit_after},
    {"any?", &bitvector_method_any},
    {"all?", &bitvector_method_all},
    {"none?", &bitvector_method_none},
    {"reverse", &bitvector_method_reverse},
    {"asUnsignedInteger", &bitvector_method_as_unsigned_integer},
    {"asSignedInteger", &bitvector_method_as_signed_integer},
    {"asList", &bitvector_method_as_list},
    {"asLEByteVector", &bitvector_method_as_le_bytevector},
    {"asBEByteVector", &bitvector_method_as_be_bytevector},
    {"asIterator", &bitvector_method_as_iterator},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor BitVector_class_methods[] = {
    {"fromUnsignedInteger:", &bitvector_class_method_from_unsigned_integer},
    {"fromUnsignedInteger:size:", &bitvector_class_method_from_unsigned_integer_size},
    {"fromInteger:size:", &bitvector_class_method_from_integer_size},
    {"fromList:", &bitvector_class_method_from_list},
    {"fromLEByteVector:", &bitvector_class_method_from_le_bytevector},
    {"fromBEByteVector:", &bitvector_class_method_from_be_bytevector},
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
    .hash = BitVector_hash,
    .equal_p = BitVector_equal_p,
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
