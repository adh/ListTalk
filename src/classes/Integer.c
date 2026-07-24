/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Integer.h>
#include <ListTalk/classes/BigInteger.h>
#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/RationalNumber.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/error.h>

#include "BigInteger_internal.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct LT_Integer_s {
    LT_Object base;
};

static LT_Value integer_from_unsigned_bytes(const uint8_t* bytes, size_t length){
    LT_Value result = LT_SmallInteger_new(0);
    size_t i;

    for (i = 0; i < length; i++){
        result = LT_Integer_multiply(result, LT_SmallInteger_new(256));
        result = LT_Integer_add(result, LT_SmallInteger_new((int64_t)bytes[i]));
    }
    return result;
}

static LT_Value integer_pow256(size_t exponent){
    LT_Value result = LT_SmallInteger_new(1);
    size_t i;

    for (i = 0; i < exponent; i++){
        result = LT_Integer_multiply(result, LT_SmallInteger_new(256));
    }
    return result;
}

static uint8_t integer_low_byte(LT_Value value){
    uint32_t byte;

    if (!LT_Integer_to_uint32(value, &byte) || byte > UINT8_MAX){
        LT_error("Integer byte conversion failed");
    }
    return (uint8_t)byte;
}

static void integer_divmod_byte(LT_Value value,
                                LT_Value* quotient,
                                uint8_t* remainder){
    LT_Value remainder_value;

    LT_Integer_divmod(
        value,
        LT_SmallInteger_new(256),
        quotient,
        &remainder_value
    );
    *remainder = integer_low_byte(remainder_value);
}

static LT_ByteVector* integer_to_unsigned_bytes(LT_Value value,
                                                int fixed_width,
                                                size_t byte_count){
    LT_Value cursor;
    uint8_t* bytes;
    size_t length;
    size_t index;

    if (!LT_Integer_value_p(value)){
        LT_type_error(value, &LT_Integer_class);
    }
    if (LT_Integer_negative_p(value)){
        LT_error("Cannot encode negative integer as unsigned bytes");
    }

    if (fixed_width){
        bytes = GC_MALLOC_ATOMIC(byte_count == 0 ? 1 : byte_count);
        memset(bytes, 0, byte_count);
        cursor = value;
        index = byte_count;
        while (!LT_Integer_is_zero(cursor) && index > 0){
            uint8_t byte;
            LT_Value quotient;

            integer_divmod_byte(cursor, &quotient, &byte);
            bytes[--index] = byte;
            cursor = quotient;
        }
        if (!LT_Integer_is_zero(cursor)){
            LT_error("Integer does not fit requested byte count");
        }
        return LT_ByteVector_new(bytes, byte_count);
    }

    if (LT_Integer_is_zero(value)){
        uint8_t zero = 0;

        return LT_ByteVector_new(&zero, 1);
    }

    length = 0;
    cursor = value;
    while (!LT_Integer_is_zero(cursor)){
        LT_Value quotient;
        uint8_t byte;

        integer_divmod_byte(cursor, &quotient, &byte);
        (void)byte;
        cursor = quotient;
        length++;
    }

    bytes = GC_MALLOC_ATOMIC(length);
    cursor = value;
    index = length;
    while (index > 0){
        uint8_t byte;
        LT_Value quotient;

        integer_divmod_byte(cursor, &quotient, &byte);
        bytes[--index] = byte;
        cursor = quotient;
    }
    return LT_ByteVector_new(bytes, length);
}

static LT_Value twos_complement_modulus(size_t byte_count){
    return integer_pow256(byte_count);
}

static LT_Value twos_complement_positive_limit(size_t byte_count){
    LT_Value limit;

    if (byte_count == 0){
        LT_error("Two's-complement byte count must be positive");
    }
    limit = integer_pow256(byte_count - 1);
    return LT_Integer_multiply(limit, LT_SmallInteger_new(128));
}

static LT_ByteVector* integer_to_twos_complement_bytes(LT_Value value,
                                                       int fixed_width,
                                                       size_t byte_count){
    LT_Value encoded = value;
    size_t length = byte_count;

    if (!LT_Integer_value_p(value)){
        LT_type_error(value, &LT_Integer_class);
    }

    if (fixed_width){
        LT_Value positive_limit = twos_complement_positive_limit(byte_count);
        LT_Value min_value = LT_Integer_negate(positive_limit);
        LT_Value max_value = LT_Integer_subtract(
            positive_limit,
            LT_SmallInteger_new(1)
        );

        if (LT_Integer_compare(value, min_value) < 0
            || LT_Integer_compare(value, max_value) > 0){
            LT_error("Integer does not fit requested two's-complement byte count");
        }
    } else if (LT_Integer_negative_p(value)){
        LT_Value positive_limit = LT_SmallInteger_new(128);

        length = 1;
        while (LT_Integer_compare(value, LT_Integer_negate(positive_limit)) < 0){
            positive_limit = LT_Integer_multiply(
                positive_limit,
                LT_SmallInteger_new(256)
            );
            length++;
        }
    } else {
        LT_ByteVector* unsigned_bytes = integer_to_unsigned_bytes(value, 0, 0);
        const uint8_t* bytes = LT_ByteVector_bytes(unsigned_bytes);
        size_t unsigned_length = LT_ByteVector_length(unsigned_bytes);

        if (unsigned_length > 0 && (bytes[0] & 0x80) != 0){
            uint8_t* padded = GC_MALLOC_ATOMIC(unsigned_length + 1);

            padded[0] = 0;
            memcpy(padded + 1, bytes, unsigned_length);
            return LT_ByteVector_new(padded, unsigned_length + 1);
        }
        return unsigned_bytes;
    }

    if (LT_Integer_negative_p(value)){
        encoded = LT_Integer_add(value, twos_complement_modulus(length));
    }
    return integer_to_unsigned_bytes(encoded, 1, length);
}

LT_DEFINE_PRIMITIVE(
    integer_method_to_bytes,
    "Integer>>toBytes",
    "(self)",
    "Return the shortest big-endian unsigned byte representation of receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    return (LT_Value)(uintptr_t)integer_to_unsigned_bytes(self, 0, 0);
}

LT_DEFINE_PRIMITIVE(
    integer_method_to_bytes_count,
    "Integer>>toBytes:",
    "(self byte-count)",
    "Return a big-endian unsigned byte representation with byte-count bytes."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value byte_count_value;
    size_t byte_count;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, byte_count_value);
    LT_ARG_END(cursor);

    byte_count = LT_Number_nonnegative_size_from_integer(
        byte_count_value,
        "Byte count must be non-negative",
        "Byte count out of range"
    );
    return (LT_Value)(uintptr_t)integer_to_unsigned_bytes(self, 1, byte_count);
}

LT_DEFINE_PRIMITIVE(
    integer_method_to_twos_complement,
    "Integer>>toTwosComplement",
    "(self)",
    "Return the shortest big-endian two's-complement byte representation of receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    return (LT_Value)(uintptr_t)integer_to_twos_complement_bytes(self, 0, 0);
}

LT_DEFINE_PRIMITIVE(
    integer_method_to_twos_complement_count,
    "Integer>>toTwosComplement:",
    "(self byte-count)",
    "Return a big-endian two's-complement byte representation with byte-count bytes."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value byte_count_value;
    size_t byte_count;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, byte_count_value);
    LT_ARG_END(cursor);

    byte_count = LT_Number_nonnegative_size_from_integer(
        byte_count_value,
        "Byte count must be non-negative",
        "Byte count out of range"
    );
    return (LT_Value)(uintptr_t)integer_to_twos_complement_bytes(
        self,
        1,
        byte_count
    );
}

LT_DEFINE_PRIMITIVE(
    integer_class_method_from_bytes,
    "Integer class>>fromBytes:",
    "(self bytevector)",
    "Return an integer decoded from big-endian unsigned bytes."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_ByteVector* bytevector;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, bytevector, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    (void)self;

    return integer_from_unsigned_bytes(
        LT_ByteVector_bytes(bytevector),
        LT_ByteVector_length(bytevector)
    );
}

LT_DEFINE_PRIMITIVE(
    integer_class_method_from_twos_complement,
    "Integer class>>fromTwosComplement:",
    "(self bytevector)",
    "Return an integer decoded from big-endian two's-complement bytes."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_ByteVector* bytevector;
    const uint8_t* bytes;
    size_t length;
    LT_Value unsigned_value;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, bytevector, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    (void)self;

    bytes = LT_ByteVector_bytes(bytevector);
    length = LT_ByteVector_length(bytevector);
    unsigned_value = integer_from_unsigned_bytes(bytes, length);

    if (length == 0 || (bytes[0] & 0x80) == 0){
        return unsigned_value;
    }
    return LT_Integer_subtract(unsigned_value, twos_complement_modulus(length));
}

LT_DEFINE_PRIMITIVE(
    integer_method_times_do,
    "Integer>>timesDo:",
    "(self callable)",
    "Call callable with each integer index from zero up to receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    LT_Value index;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);

    index = LT_SmallInteger_new(0);
    while (LT_Integer_compare(index, self) < 0){
        (void)LT_APPLY(callable, index);
        index = LT_Integer_add(index, LT_SmallInteger_new(1));
    }
    return LT_NIL;
}

static LT_Method_Descriptor Integer_methods[] = {
    {"timesDo:", &integer_method_times_do},
    {"toBytes", &integer_method_to_bytes},
    {"toBytes:", &integer_method_to_bytes_count},
    {"toTwosComplement", &integer_method_to_twos_complement},
    {"toTwosComplement:", &integer_method_to_twos_complement_count},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor Integer_class_methods[] = {
    {"fromBytes:", &integer_class_method_from_bytes},
    {"fromTwosComplement:", &integer_class_method_from_twos_complement},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Integer) {
    .superclass = &LT_RationalNumber_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Integer",
    .documentation = "Abstract root for exact integer values.",
    .instance_size = sizeof(LT_Integer),
    .class_flags = LT_CLASS_FLAG_ABSTRACT
        | LT_CLASS_FLAG_IMMUTABLE
        | LT_CLASS_FLAG_SCALAR,
    .methods = Integer_methods,
    .class_methods = Integer_class_methods,
};

LT_Value LT_Integer_from_intmax(intmax_t value){
    uintmax_t magnitude;
    uint32_t limbs[sizeof(uintmax_t) / sizeof(uint32_t)];
    size_t limb_count = 0;
    int negative = value < 0;

    if (value >= (intmax_t)LT_VALUE_FIXNUM_MIN
        && value <= (intmax_t)LT_VALUE_FIXNUM_MAX){
        return LT_SmallInteger_new((int64_t)value);
    }

    magnitude = negative
        ? (uintmax_t)(-(value + 1)) + 1
        : (uintmax_t)value;

    while (magnitude != 0){
        limbs[limb_count++] = (uint32_t)magnitude;
        magnitude >>= 32;
    }

    return LT_BigInteger_new_from_limbs(negative, limb_count, limbs);
}

LT_Value LT_Integer_from_uintmax(uintmax_t value){
    uint32_t limbs[sizeof(uintmax_t) / sizeof(uint32_t)];
    size_t limb_count = 0;

    if (value <= (uintmax_t)LT_VALUE_FIXNUM_MAX){
        return LT_SmallInteger_new((int64_t)value);
    }

    while (value != 0){
        limbs[limb_count++] = (uint32_t)value;
        value >>= 32;
    }

    return LT_BigInteger_new_from_limbs(0, limb_count, limbs);
}
