/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/macros/arg_macros.h>

#include <stdint.h>
#include <string.h>

struct LT_ByteVector_s {
    LT_Object base;
    size_t length;
    uint8_t bytes[];
};

static size_t ByteVector_hash(LT_Value value){
    LT_ByteVector* bytevector = LT_ByteVector_from_value(value);
    size_t length = LT_ByteVector_length(bytevector);
    const uint8_t* bytes = LT_ByteVector_bytes(bytevector);
    uint32_t hash = UINT32_C(0x811c9dc5);
    size_t i;

    for (i = 0; i < length; i++){
        hash += (uint32_t)bytes[i];
        hash *= UINT32_C(0x01000193);
    }
    return (size_t)hash;
}

static int ByteVector_equal_p(LT_Value left, LT_Value right){
    LT_ByteVector* left_bytevector;
    LT_ByteVector* right_bytevector;
    size_t length;

    if (!LT_ByteVector_p(right)){
        return 0;
    }

    left_bytevector = LT_ByteVector_from_value(left);
    right_bytevector = LT_ByteVector_from_value(right);
    length = LT_ByteVector_length(left_bytevector);

    if (length != LT_ByteVector_length(right_bytevector)){
        return 0;
    }
    return memcmp(
        LT_ByteVector_bytes(left_bytevector),
        LT_ByteVector_bytes(right_bytevector),
        length
    ) == 0;
}

static void ByteVector_debugPrintOn(LT_Value obj, FILE* stream){
    LT_ByteVector* bytevector = LT_ByteVector_from_value(obj);
    size_t i;

    fputs("#\"", stream);
    for (i = 0; i < bytevector->length; i++){
        uint8_t byte = bytevector->bytes[i];

        switch (byte){
            case '\a':
                fputs("\\a", stream);
                break;
            case '\b':
                fputs("\\b", stream);
                break;
            case '\f':
                fputs("\\f", stream);
                break;
            case '\n':
                fputs("\\n", stream);
                break;
            case '\r':
                fputs("\\r", stream);
                break;
            case '\t':
                fputs("\\t", stream);
                break;
            case '\v':
                fputs("\\v", stream);
                break;
            case '\\':
                fputs("\\\\", stream);
                break;
            case '"':
                fputs("\\\"", stream);
                break;
            default:
                if (byte >= 0x20 && byte <= 0x7e){
                    fputc((int)byte, stream);
                } else {
                    fprintf(stream, "\\x%02x", (unsigned int)byte);
                }
                break;
        }
    }
    fputc('"', stream);
}

LT_DEFINE_PRIMITIVE(
    bytevector_method_length,
    "ByteVector>>length",
    "(self)",
    "Return bytevector length in bytes."
){
    LT_Value cursor = arguments;
    LT_Value self;
    size_t length;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    length = LT_ByteVector_length(LT_ByteVector_from_value(self));
    return LT_Number_smallinteger_from_size(
        length,
        "ByteVector length does not fit fixnum"
    );
}

LT_DEFINE_PRIMITIVE(
    bytevector_method_at,
    "ByteVector>>at:",
    "(self index)",
    "Return byte at index as an unsigned fixnum."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value index;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, index);
    LT_ARG_END(cursor);

    return LT_SmallInteger_new((int64_t)LT_ByteVector_at(
        LT_ByteVector_from_value(self),
        LT_Number_nonnegative_size_from_integer(
            index,
            "ByteVector index out of bounds",
            "ByteVector index out of bounds"
        )
    ));
}

LT_DEFINE_PRIMITIVE(
    bytevector_method_at_put,
    "ByteVector>>at:put:",
    "(self index byte)",
    "Set byte at index and return byte as an unsigned fixnum."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value index;
    LT_Value byte;
    uint8_t byte_value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, index);
    LT_OBJECT_ARG(cursor, byte);
    LT_ARG_END(cursor);

    byte_value = LT_Number_uint8_from_integer(byte, "Byte value out of range");
    LT_ByteVector_atPut(
        LT_ByteVector_from_value(self),
        LT_Number_nonnegative_size_from_integer(
            index,
            "ByteVector index out of bounds",
            "ByteVector index out of bounds"
        ),
        byte_value
    );
    return LT_SmallInteger_new((int64_t)byte_value);
}

LT_DEFINE_PRIMITIVE(
    bytevector_method_append,
    "ByteVector>>append:",
    "(self other)",
    "Return a new bytevector with other appended."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_ByteVector* other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, other, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);

    return (LT_Value)(uintptr_t)LT_ByteVector_append(
        LT_ByteVector_from_value(self),
        other
    );
}

LT_DEFINE_PRIMITIVE(
    bytevector_method_from_to,
    "ByteVector>>from:to:",
    "(self from to)",
    "Return a bytevector slice using half-open byte indexes."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value from;
    LT_Value to;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, from);
    LT_OBJECT_ARG(cursor, to);
    LT_ARG_END(cursor);

    return (LT_Value)(uintptr_t)LT_ByteVector_from_to(
        LT_ByteVector_from_value(self),
        LT_Number_nonnegative_size_from_integer(
            from,
            "ByteVector index out of bounds",
            "ByteVector index out of bounds"
        ),
        LT_Number_nonnegative_size_from_integer(
            to,
            "ByteVector index out of bounds",
            "ByteVector index out of bounds"
        )
    );
}

LT_DEFINE_PRIMITIVE(
    bytevector_method_as_string,
    "ByteVector>>asString",
    "(self)",
    "Return a string decoded from this bytevector."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    return (LT_Value)(uintptr_t)LT_ByteVector_to_string(
        LT_ByteVector_from_value(self)
    );
}

static LT_Method_Descriptor ByteVector_methods[] = {
    {"length", &bytevector_method_length},
    {"at:", &bytevector_method_at},
    {"at:put:", &bytevector_method_at_put},
    {"append:", &bytevector_method_append},
    {"from:to:", &bytevector_method_from_to},
    {"asString", &bytevector_method_as_string},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_ByteVector) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "ByteVector",
    .instance_size = sizeof(LT_ByteVector),
    .class_flags = LT_CLASS_FLAG_FLEXIBLE,
    .hash = ByteVector_hash,
    .equal_p = ByteVector_equal_p,
    .debugPrintOn = ByteVector_debugPrintOn,
    .methods = ByteVector_methods,
};

LT_ByteVector* LT_ByteVector_new(const uint8_t* bytes, size_t length){
    LT_ByteVector* bytevector = LT_Class_ALLOC_FLEXIBLE(
        LT_ByteVector,
        sizeof(uint8_t) * length
    );

    bytevector->length = length;
    if (length > 0){
        memcpy(bytevector->bytes, bytes, length);
    }
    return bytevector;
}

LT_ByteVector* LT_ByteVector_new_filled(size_t length, uint8_t fill){
    LT_ByteVector* bytevector = LT_Class_ALLOC_FLEXIBLE(
        LT_ByteVector,
        sizeof(uint8_t) * length
    );

    bytevector->length = length;
    memset(bytevector->bytes, fill, length);
    return bytevector;
}

LT_ByteVector* LT_ByteVector_from_string(LT_String* string){
    return LT_ByteVector_new(
        (const uint8_t*)LT_String_value_cstr(string),
        LT_String_byte_length(string)
    );
}

LT_String* LT_ByteVector_to_string(LT_ByteVector* bytevector){
    return LT_String_new(
        (char*)LT_ByteVector_bytes(bytevector),
        LT_ByteVector_length(bytevector)
    );
}

LT_ByteVector* LT_ByteVector_append(LT_ByteVector* left,
                                    LT_ByteVector* right){
    size_t left_length = LT_ByteVector_length(left);
    size_t right_length = LT_ByteVector_length(right);
    LT_ByteVector* result = LT_Class_ALLOC_FLEXIBLE(
        LT_ByteVector,
        sizeof(uint8_t) * (left_length + right_length)
    );

    result->length = left_length + right_length;
    memcpy(result->bytes, LT_ByteVector_bytes(left), left_length);
    memcpy(
        result->bytes + left_length,
        LT_ByteVector_bytes(right),
        right_length
    );
    return result;
}

LT_ByteVector* LT_ByteVector_from_to(LT_ByteVector* bytevector,
                                     size_t from,
                                     size_t to){
    if (from > to || to > LT_ByteVector_length(bytevector)){
        LT_error("ByteVector index out of bounds");
    }
    return LT_ByteVector_new(
        LT_ByteVector_bytes(bytevector) + from,
        to - from
    );
}

size_t LT_ByteVector_length(LT_ByteVector* bytevector){
    return bytevector->length;
}

uint8_t LT_ByteVector_at(LT_ByteVector* bytevector, size_t index){
    if (index >= bytevector->length){
        LT_error("ByteVector index out of bounds");
    }
    return bytevector->bytes[index];
}

void LT_ByteVector_atPut(LT_ByteVector* bytevector,
                         size_t index,
                         uint8_t value){
    if (index >= bytevector->length){
        LT_error("ByteVector index out of bounds");
    }
    bytevector->bytes[index] = value;
}

const uint8_t* LT_ByteVector_bytes(LT_ByteVector* bytevector){
    return bytevector->bytes;
}
