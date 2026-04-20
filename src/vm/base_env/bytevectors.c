/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/error.h>

#include <stdint.h>

static uint8_t checked_byte_from_fixnum(int64_t value){
    if (value < 0 || value > UINT8_MAX){
        LT_error("Byte value out of range");
    }
    return (uint8_t)value;
}

LT_DEFINE_PRIMITIVE(
    primitive_bytevector_p,
    "bytevector?",
    "(value)",
    "Return true when value is a bytevector."
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_ByteVector_p(value) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    primitive_bytevector_length,
    "bytevector-length",
    "(bytevector)",
    "Return bytevector length in bytes as fixnum."
){
    LT_Value cursor = arguments;
    LT_ByteVector* bytevector;
    size_t length;

    LT_GENERIC_ARG(cursor, bytevector, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);

    length = LT_ByteVector_length(bytevector);
    if (!LT_SmallInteger_in_range((int64_t)length)){
        LT_error("ByteVector length does not fit fixnum");
    }
    return LT_SmallInteger_new((int64_t)length);
}

LT_DEFINE_PRIMITIVE(
    primitive_bytevector_u8_ref,
    "bytevector-u8-ref",
    "(bytevector index)",
    "Return byte at index as an unsigned fixnum."
){
    LT_Value cursor = arguments;
    LT_ByteVector* bytevector;
    int64_t index_value;

    LT_GENERIC_ARG(cursor, bytevector, LT_ByteVector*, LT_ByteVector_from_value);
    LT_FIXNUM_ARG(cursor, index_value);
    LT_ARG_END(cursor);

    return LT_SmallInteger_new((int64_t)LT_ByteVector_at(
        bytevector,
        checked_nonnegative_from_fixnum(index_value)
    ));
}

LT_DEFINE_PRIMITIVE(
    primitive_bytevector_u8_set_bang,
    "bytevector-u8-set!",
    "(bytevector index byte)",
    "Set byte at index and return byte as an unsigned fixnum."
){
    LT_Value cursor = arguments;
    LT_ByteVector* bytevector;
    int64_t index_value;
    int64_t byte_value;
    uint8_t byte;

    LT_GENERIC_ARG(cursor, bytevector, LT_ByteVector*, LT_ByteVector_from_value);
    LT_FIXNUM_ARG(cursor, index_value);
    LT_FIXNUM_ARG(cursor, byte_value);
    LT_ARG_END(cursor);

    byte = checked_byte_from_fixnum(byte_value);
    LT_ByteVector_atPut(
        bytevector,
        checked_nonnegative_from_fixnum(index_value),
        byte
    );
    return LT_SmallInteger_new((int64_t)byte);
}

LT_DEFINE_PRIMITIVE(
    primitive_make_bytevector,
    "make-bytevector",
    "(length [fill])",
    "Create a bytevector with optional byte fill."
){
    LT_Value cursor = arguments;
    int64_t length_value;
    int64_t fill_value = 0;

    LT_FIXNUM_ARG(cursor, length_value);
    if (cursor != LT_NIL){
        LT_FIXNUM_ARG(cursor, fill_value);
    }
    LT_ARG_END(cursor);

    return (LT_Value)(uintptr_t)LT_ByteVector_new_filled(
        checked_nonnegative_from_fixnum(length_value),
        checked_byte_from_fixnum(fill_value)
    );
}

LT_DEFINE_PRIMITIVE(
    primitive_bytevector,
    "bytevector",
    "(byte ...)",
    "Create bytevector from unsigned byte fixnums."
){
    LT_Value cursor = arguments;
    LT_Value values;
    LT_ByteVector* bytevector;
    size_t length = 0;
    size_t index = 0;

    LT_ARG_REST(cursor, values);
    while (cursor != LT_NIL){
        if (!LT_Pair_p(cursor)){
            LT_error("Malformed argument list while creating bytevector");
        }
        checked_byte_from_fixnum(LT_SmallInteger_value(LT_car(cursor)));
        length++;
        cursor = LT_cdr(cursor);
    }

    bytevector = LT_ByteVector_new_filled(length, 0);
    cursor = values;
    while (cursor != LT_NIL){
        LT_ByteVector_atPut(
            bytevector,
            index,
            checked_byte_from_fixnum(LT_SmallInteger_value(LT_car(cursor)))
        );
        index++;
        cursor = LT_cdr(cursor);
    }
    return (LT_Value)(uintptr_t)bytevector;
}

LT_DEFINE_PRIMITIVE(
    primitive_bytevector_append,
    "bytevector-append",
    "(bytevector ...)",
    "Concatenate all bytevector arguments."
){
    LT_Value cursor = arguments;
    LT_ByteVector* result = LT_ByteVector_new_filled(0, 0);

    while (cursor != LT_NIL){
        LT_ByteVector* bytevector;

        LT_GENERIC_ARG(cursor,
                       bytevector,
                       LT_ByteVector*,
                       LT_ByteVector_from_value);
        result = LT_ByteVector_append(result, bytevector);
    }
    return (LT_Value)(uintptr_t)result;
}

LT_DEFINE_PRIMITIVE(
    primitive_bytevector_copy,
    "bytevector-copy",
    "(bytevector [from [to]])",
    "Return a bytevector slice using half-open byte indexes."
){
    LT_Value cursor = arguments;
    LT_ByteVector* bytevector;
    int64_t from_value = 0;
    int64_t to_value;

    LT_GENERIC_ARG(cursor, bytevector, LT_ByteVector*, LT_ByteVector_from_value);
    to_value = (int64_t)LT_ByteVector_length(bytevector);
    if (cursor != LT_NIL){
        LT_FIXNUM_ARG(cursor, from_value);
    }
    if (cursor != LT_NIL){
        LT_FIXNUM_ARG(cursor, to_value);
    }
    LT_ARG_END(cursor);

    return (LT_Value)(uintptr_t)LT_ByteVector_from_to(
        bytevector,
        checked_nonnegative_from_fixnum(from_value),
        checked_nonnegative_from_fixnum(to_value)
    );
}

LT_DEFINE_PRIMITIVE(
    primitive_string_to_bytevector,
    "string->bytevector",
    "(string)",
    "Return a bytevector containing string UTF-8 bytes."
){
    LT_Value cursor = arguments;
    LT_String* string;

    LT_GENERIC_ARG(cursor, string, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);

    return (LT_Value)(uintptr_t)LT_ByteVector_from_string(string);
}

LT_DEFINE_PRIMITIVE(
    primitive_bytevector_to_string,
    "bytevector->string",
    "(bytevector)",
    "Return a string decoded from bytevector bytes."
){
    LT_Value cursor = arguments;
    LT_ByteVector* bytevector;

    LT_GENERIC_ARG(cursor, bytevector, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);

    return (LT_Value)(uintptr_t)LT_ByteVector_to_string(bytevector);
}

void LT_base_env_bind_bytevectors(LT_Environment* environment){
    LT_base_env_bind_static_primitive(environment, &primitive_bytevector_p);
    LT_base_env_bind_static_primitive(environment, &primitive_bytevector_length);
    LT_base_env_bind_static_primitive(environment, &primitive_bytevector_u8_ref);
    LT_base_env_bind_static_primitive(
        environment,
        &primitive_bytevector_u8_set_bang
    );
    LT_base_env_bind_static_primitive(environment, &primitive_make_bytevector);
    LT_base_env_bind_static_primitive(environment, &primitive_bytevector);
    LT_base_env_bind_static_primitive(environment, &primitive_bytevector_append);
    LT_base_env_bind_static_primitive(environment, &primitive_bytevector_copy);
    LT_base_env_bind_static_primitive(
        environment,
        &primitive_string_to_bytevector
    );
    LT_base_env_bind_static_primitive(
        environment,
        &primitive_bytevector_to_string
    );
}
