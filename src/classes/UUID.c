/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/UUID.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct LT_UUID_s {
    LT_Object base;
    uint8_t bytes[LT_UUID_BYTE_LENGTH];
};

static int UUID_hex_value(char ch){
    if (ch >= '0' && ch <= '9'){
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f'){
        return 10 + ch - 'a';
    }
    if (ch >= 'A' && ch <= 'F'){
        return 10 + ch - 'A';
    }
    return -1;
}

static int UUID_parse_cstr(const char* text, uint8_t bytes[LT_UUID_BYTE_LENGTH]){
    size_t text_index = 0;
    size_t byte_index;

    for (byte_index = 0; byte_index < LT_UUID_BYTE_LENGTH; byte_index++){
        int high;
        int low;

        if (text_index == 8
            || text_index == 13
            || text_index == 18
            || text_index == 23){
            if (text[text_index] != '-'){
                return 0;
            }
            text_index++;
        }

        high = UUID_hex_value(text[text_index++]);
        low = UUID_hex_value(text[text_index++]);
        if (high < 0 || low < 0){
            return 0;
        }
        bytes[byte_index] = (uint8_t)((high << 4) | low);
    }
    return text[text_index] == '\0';
}

static void UUID_read_random_bytes(uint8_t bytes[LT_UUID_BYTE_LENGTH]){
    size_t offset = 0;
    int fd = open("/dev/urandom", O_RDONLY);

    if (fd < 0){
        LT_system_error("Could not open /dev/urandom", errno);
    }

    while (offset < LT_UUID_BYTE_LENGTH){
        ssize_t bytes_read = read(
            fd,
            bytes + offset,
            LT_UUID_BYTE_LENGTH - offset
        );

        if (bytes_read < 0){
            int saved_errno = errno;

            close(fd);
            LT_system_error("Could not read /dev/urandom", saved_errno);
        }
        if (bytes_read == 0){
            close(fd);
            LT_error("Could not read enough random data");
        }
        offset += (size_t)bytes_read;
    }

    if (close(fd) != 0){
        LT_system_error("Could not close /dev/urandom", errno);
    }
}

static size_t UUID_hash(LT_Value value){
    LT_UUID* uuid = LT_UUID_from_value(value);
    uint32_t hash = UINT32_C(0x811c9dc5);
    size_t i;

    for (i = 0; i < LT_UUID_BYTE_LENGTH; i++){
        hash += (uint32_t)uuid->bytes[i];
        hash *= UINT32_C(0x01000193);
    }
    return (size_t)hash;
}

static int UUID_equal_p(LT_Value left, LT_Value right){
    LT_UUID* left_uuid;
    LT_UUID* right_uuid;

    if (!LT_UUID_p(right)){
        return 0;
    }

    left_uuid = LT_UUID_from_value(left);
    right_uuid = LT_UUID_from_value(right);
    return memcmp(
        LT_UUID_bytes(left_uuid),
        LT_UUID_bytes(right_uuid),
        LT_UUID_BYTE_LENGTH
    ) == 0;
}

static void UUID_debugPrintOn(LT_Value obj, FILE* stream){
    LT_UUID* uuid = LT_UUID_from_value(obj);
    LT_String* string = LT_UUID_as_string(uuid);

    fprintf(stream, "#<UUID %s>", LT_String_value_cstr(string));
}

LT_DEFINE_PRIMITIVE(
    uuid_class_method_random_v4,
    "UUID class>>randomV4",
    "(self)",
    "Return a random RFC 4122 version 4 UUID."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_UUID_class){
        LT_error("randomV4 class method is only supported on UUID");
    }
    return (LT_Value)(uintptr_t)LT_UUID_random_v4();
}

LT_DEFINE_PRIMITIVE(
    uuid_class_method_from_string,
    "UUID class>>fromString:",
    "(self string)",
    "Return a UUID parsed from its canonical string representation."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* string;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, string, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_UUID_class){
        LT_error("fromString: class method is only supported on UUID");
    }
    return (LT_Value)(uintptr_t)LT_UUID_from_string(string);
}

LT_DEFINE_PRIMITIVE(
    uuid_class_method_from_bytevector,
    "UUID class>>fromByteVector:",
    "(self bytevector)",
    "Return a UUID from a 16-byte bytevector."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_ByteVector* bytevector;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, bytevector, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_UUID_class){
        LT_error("fromByteVector: class method is only supported on UUID");
    }
    if (LT_ByteVector_length(bytevector) != LT_UUID_BYTE_LENGTH){
        LT_error("UUID bytevector must be exactly 16 bytes");
    }
    return (LT_Value)(uintptr_t)LT_UUID_new(LT_ByteVector_bytes(bytevector));
}

LT_DEFINE_PRIMITIVE(
    uuid_method_as_string,
    "UUID>>asString",
    "(self)",
    "Return receiver as a canonical lowercase UUID string."
){
    LT_Value cursor = arguments;
    LT_UUID* uuid;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, uuid, LT_UUID*, LT_UUID_from_value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_UUID_as_string(uuid);
}

LT_DEFINE_PRIMITIVE(
    uuid_method_as_bytevector,
    "UUID>>asByteVector",
    "(self)",
    "Return receiver bytes as a bytevector."
){
    LT_Value cursor = arguments;
    LT_UUID* uuid;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, uuid, LT_UUID*, LT_UUID_from_value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_ByteVector_new(
        LT_UUID_bytes(uuid),
        LT_UUID_BYTE_LENGTH
    );
}

LT_DEFINE_PRIMITIVE(
    uuid_method_at,
    "UUID>>at:",
    "(self index)",
    "Return byte at zero-based index."
){
    LT_Value cursor = arguments;
    LT_UUID* uuid;
    int64_t index;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, uuid, LT_UUID*, LT_UUID_from_value);
    LT_FIXNUM_ARG(cursor, index);
    LT_ARG_END(cursor);
    if (index < 0 || index >= LT_UUID_BYTE_LENGTH){
        LT_error("UUID index out of bounds");
    }
    return LT_SmallInteger_new((int64_t)LT_UUID_at(uuid, (size_t)index));
}

LT_DEFINE_PRIMITIVE(
    uuid_method_compare_with,
    "UUID>>compareWith:",
    "(self other)",
    "Return -1, 0, or 1 comparing UUID bytes lexicographically."
){
    LT_Value cursor = arguments;
    LT_UUID* self;
    LT_UUID* other;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, self, LT_UUID*, LT_UUID_from_value);
    LT_GENERIC_ARG(cursor, other, LT_UUID*, LT_UUID_from_value);
    LT_ARG_END(cursor);
    return LT_SmallInteger_new((int64_t)LT_UUID_compare(self, other));
}

static LT_Method_Descriptor UUID_methods[] = {
    {"asString", &uuid_method_as_string},
    {"asByteVector", &uuid_method_as_bytevector},
    {"at:", &uuid_method_at},
    {"compareWith:", &uuid_method_compare_with},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor UUID_class_methods[] = {
    {"randomV4", &uuid_class_method_random_v4},
    {"fromString:", &uuid_class_method_from_string},
    {"fromByteVector:", &uuid_class_method_from_bytevector},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_UUID) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "UUID",
    .documentation = "Immutable 128-bit universally unique identifier.",
    .instance_size = sizeof(LT_UUID),
    .class_flags = LT_CLASS_FLAG_IMMUTABLE | LT_CLASS_FLAG_SCALAR,
    .hash = UUID_hash,
    .equal_p = UUID_equal_p,
    .debugPrintOn = UUID_debugPrintOn,
    .methods = UUID_methods,
    .class_methods = UUID_class_methods,
};

LT_UUID* LT_UUID_new(const uint8_t bytes[LT_UUID_BYTE_LENGTH]){
    LT_UUID* uuid = LT_Class_ALLOC(LT_UUID);

    memcpy(uuid->bytes, bytes, LT_UUID_BYTE_LENGTH);
    return uuid;
}

LT_UUID* LT_UUID_random_v4(void){
    uint8_t bytes[LT_UUID_BYTE_LENGTH];

    UUID_read_random_bytes(bytes);
    bytes[6] = (uint8_t)((bytes[6] & 0x0f) | 0x40);
    bytes[8] = (uint8_t)((bytes[8] & 0x3f) | 0x80);
    return LT_UUID_new(bytes);
}

LT_UUID* LT_UUID_from_string(LT_String* string){
    uint8_t bytes[LT_UUID_BYTE_LENGTH];

    if (LT_String_byte_length(string) != LT_UUID_STRING_LENGTH
        || LT_String_length(string) != LT_UUID_STRING_LENGTH
        || !UUID_parse_cstr(LT_String_value_cstr(string), bytes)){
        LT_error("Invalid UUID string");
    }
    return LT_UUID_new(bytes);
}

LT_String* LT_UUID_as_string(LT_UUID* uuid){
    static const char hex[] = "0123456789abcdef";
    char buffer[LT_UUID_STRING_LENGTH];
    size_t buffer_index = 0;
    size_t byte_index;

    for (byte_index = 0; byte_index < LT_UUID_BYTE_LENGTH; byte_index++){
        uint8_t byte = uuid->bytes[byte_index];

        if (buffer_index == 8
            || buffer_index == 13
            || buffer_index == 18
            || buffer_index == 23){
            buffer[buffer_index++] = '-';
        }
        buffer[buffer_index++] = hex[byte >> 4];
        buffer[buffer_index++] = hex[byte & 0x0f];
    }

    return LT_String_new(buffer, LT_UUID_STRING_LENGTH);
}

int LT_UUID_compare(LT_UUID* left, LT_UUID* right){
    int result = memcmp(
        LT_UUID_bytes(left),
        LT_UUID_bytes(right),
        LT_UUID_BYTE_LENGTH
    );

    if (result < 0){
        return -1;
    }
    if (result > 0){
        return 1;
    }
    return 0;
}

uint8_t LT_UUID_at(LT_UUID* uuid, size_t index){
    if (index >= LT_UUID_BYTE_LENGTH){
        LT_error("UUID index out of bounds");
    }
    return uuid->bytes[index];
}

const uint8_t* LT_UUID_bytes(LT_UUID* uuid){
    return uuid->bytes;
}
