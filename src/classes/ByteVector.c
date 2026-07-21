/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/Iterator.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Boolean.h>
#include <ListTalk/classes/List.h>
#include <ListTalk/utils.h>
#include <ListTalk/utils/base64.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/macros/arg_macros.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct LT_ByteVector_s {
    LT_Object base;
    size_t length;
    uint8_t bytes[];
};

struct LT_ByteVectorIterator_s {
    LT_Object base;
    LT_ByteVector* bytevector;
    size_t index;
};

static int comparison_sign(int comparison){
    return comparison < 0 ? -1 : (comparison > 0 ? 1 : 0);
}

static int boolean_from_value(LT_Value value, const char* message){
    if (!LT_Value_is_boolean(value)){
        LT_error(message);
    }
    return LT_Value_boolean_value(value);
}

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

static uint8_t* read_file_bytes(const char* path, size_t* length_out){
    int fd;
    LT_StringBuilder* builder;

    fd = open(path, O_RDONLY);
    if (fd < 0){
        LT_system_error("Could not open file for reading", errno);
    }

    builder = LT_StringBuilder_new();
    while (1){
        uint8_t buffer[8192];
        ssize_t bytes_read = read(fd, buffer, sizeof(buffer));

        if (bytes_read < 0){
            int saved_errno = errno;

            close(fd);
            LT_system_error("Could not read file", saved_errno);
        }
        if (bytes_read == 0){
            break;
        }
        LT_StringBuilder_append_bytes(
            builder,
            (const char*)buffer,
            (size_t)bytes_read
        );
    }
    if (close(fd) != 0){
        LT_system_error("Could not close file", errno);
    }

    *length_out = LT_StringBuilder_length(builder);
    return (uint8_t*)LT_StringBuilder_value(builder);
}

static LT_Value split_byte_lines(const uint8_t* bytes, size_t length){
    LT_ListBuilder* builder = LT_ListBuilder_new();
    size_t line_start = 0;
    size_t index = 0;

    while (index < length){
        if (bytes[index] == '\n'){
            size_t line_end = index;

            while (line_end > line_start && bytes[line_end - 1] == '\r'){
                line_end--;
            }
            LT_ListBuilder_append(
                builder,
                (LT_Value)(uintptr_t)LT_ByteVector_new(
                    bytes + line_start,
                    line_end - line_start
                )
            );
            index++;
            while (index < length && bytes[index] == '\r'){
                index++;
            }
            line_start = index;
        } else {
            index++;
        }
    }

    if (line_start < length){
        size_t line_end = length;

        while (line_end > line_start && bytes[line_end - 1] == '\r'){
            line_end--;
        }
        LT_ListBuilder_append(
            builder,
            (LT_Value)(uintptr_t)LT_ByteVector_new(
                bytes + line_start,
                line_end - line_start
            )
        );
    }

    return LT_ListBuilder_value(builder);
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

static void ByteVectorIterator_debugPrintOn(LT_Value obj, FILE* stream){
    LT_ByteVectorIterator* iterator = LT_ByteVectorIterator_from_value(obj);

    fprintf(stream, "#<ByteVectorIterator %p index=%zu>", (void*)iterator, iterator->index);
}

static LT_Value ByteVectorIterator_current(LT_ByteVectorIterator* iterator){
    if (iterator->index >= LT_ByteVector_length(iterator->bytevector)){
        LT_error("ByteVectorIterator is not positioned");
    }
    return LT_SmallInteger_new(
        (int64_t)LT_ByteVector_at(iterator->bytevector, iterator->index)
    );
}

LT_DEFINE_PRIMITIVE(
    bytevector_iterator_method_this,
    "ByteVectorIterator>>this",
    "(self)",
    "Return the current byte as an unsigned fixnum."
){
    LT_Value cursor = arguments;
    LT_ByteVectorIterator* iterator;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(
        cursor,
        iterator,
        LT_ByteVectorIterator*,
        LT_ByteVectorIterator_from_value
    );
    LT_ARG_END(cursor);
    return ByteVectorIterator_current(iterator);
}

LT_DEFINE_PRIMITIVE(
    bytevector_iterator_method_has_this,
    "ByteVectorIterator>>hasThis?",
    "(self)",
    "Return true when the iterator has a current byte."
){
    LT_Value cursor = arguments;
    LT_ByteVectorIterator* iterator;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(
        cursor,
        iterator,
        LT_ByteVectorIterator*,
        LT_ByteVectorIterator_from_value
    );
    LT_ARG_END(cursor);
    return iterator->index < LT_ByteVector_length(iterator->bytevector)
        ? LT_TRUE
        : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    bytevector_iterator_method_next,
    "ByteVectorIterator>>next!",
    "(self)",
    "Advance the iterator and return receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_ByteVectorIterator* iterator;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    iterator = LT_ByteVectorIterator_from_value(self);
    if (iterator->index < LT_ByteVector_length(iterator->bytevector)){
        iterator->index++;
    }
    return self;
}

LT_DEFINE_PRIMITIVE(
    bytevector_class_method_from_file,
    "ByteVector class>>fromFile:",
    "(self filename)",
    "Return file contents as a bytevector."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* filename;
    uint8_t* bytes;
    size_t length;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, filename, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);
    (void)self;

    bytes = read_file_bytes(LT_String_value_cstr(filename), &length);
    return (LT_Value)(uintptr_t)LT_ByteVector_new(bytes, length);
}

LT_DEFINE_PRIMITIVE(
    bytevector_method_write_to_file,
    "ByteVector>>writeToFile:",
    "(self filename)",
    "Write bytevector to a file atomically and return filename."
){
    LT_Value cursor = arguments;
    LT_ByteVector* bytevector;
    LT_String* filename;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, bytevector, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, filename, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);

    LT_write_file_bytes_atomically(
        LT_String_value_cstr(filename),
        LT_ByteVector_bytes(bytevector),
        LT_ByteVector_length(bytevector)
    );
    return (LT_Value)(uintptr_t)filename;
}

LT_DEFINE_PRIMITIVE(
    bytevector_method_split_lines,
    "ByteVector>>splitLines",
    "(self)",
    "Return bytevector lines without line terminators."
){
    LT_Value cursor = arguments;
    LT_ByteVector* bytevector;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, bytevector, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);

    return split_byte_lines(
        LT_ByteVector_bytes(bytevector),
        LT_ByteVector_length(bytevector)
    );
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
    bytevector_method_compare_with,
    "ByteVector>>compareWith:",
    "(self other)",
    "Return -1, 0, or 1 when receiver is lexicographically less than, equal to, or greater than argument."
){
    LT_Value cursor = arguments;
    LT_ByteVector* self;
    LT_ByteVector* other;
    int comparison;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, self, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, other, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);

    comparison = LT_ByteVector_compare(self, other);
    return LT_SmallInteger_new((int64_t)comparison);
}

LT_DEFINE_PRIMITIVE(
    bytevector_method_less_than,
    "ByteVector>><",
    "(self other)",
    "Return true when receiver is lexicographically less than argument."
){
    LT_Value cursor = arguments;
    LT_ByteVector* self;
    LT_ByteVector* other;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, self, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, other, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    return LT_ByteVector_compare(self, other) < 0 ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    bytevector_method_greater_than,
    "ByteVector>>>",
    "(self other)",
    "Return true when receiver is lexicographically greater than argument."
){
    LT_Value cursor = arguments;
    LT_ByteVector* self;
    LT_ByteVector* other;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, self, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, other, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    return LT_ByteVector_compare(self, other) > 0 ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    bytevector_method_less_than_or_equal,
    "ByteVector>><=",
    "(self other)",
    "Return true when receiver is lexicographically less than or equal to argument."
){
    LT_Value cursor = arguments;
    LT_ByteVector* self;
    LT_ByteVector* other;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, self, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, other, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    return LT_ByteVector_compare(self, other) <= 0 ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    bytevector_method_greater_than_or_equal,
    "ByteVector>>>=",
    "(self other)",
    "Return true when receiver is lexicographically greater than or equal to argument."
){
    LT_Value cursor = arguments;
    LT_ByteVector* self;
    LT_ByteVector* other;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, self, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, other, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    return LT_ByteVector_compare(self, other) >= 0 ? LT_TRUE : LT_FALSE;
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

LT_DEFINE_PRIMITIVE(
    bytevector_method_as_base64,
    "ByteVector>>asBase64",
    "(self)",
    "Return a base64 string encoding the receiver bytes."
){
    LT_Value cursor = arguments;
    LT_ByteVector* bytevector;
    char* encoded;
    size_t encoded_length;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, bytevector, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);

    encoded = LT_base64_encode(
        LT_ByteVector_bytes(bytevector),
        LT_ByteVector_length(bytevector),
        0,
        1,
        &encoded_length
    );
    return (LT_Value)(uintptr_t)LT_String_new(encoded, encoded_length);
}

LT_DEFINE_PRIMITIVE(
    bytevector_method_as_base64_with_padding,
    "ByteVector>>asBase64WithPadding:",
    "(self include_padding)",
    "Return a base64 string, optionally omitting padding."
){
    LT_Value cursor = arguments;
    LT_ByteVector* bytevector;
    LT_Value include_padding;
    char* encoded;
    size_t encoded_length;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, bytevector, LT_ByteVector*, LT_ByteVector_from_value);
    LT_OBJECT_ARG(cursor, include_padding);
    LT_ARG_END(cursor);

    encoded = LT_base64_encode(
        LT_ByteVector_bytes(bytevector),
        LT_ByteVector_length(bytevector),
        0,
        boolean_from_value(include_padding, "Expected boolean padding option"),
        &encoded_length
    );
    return (LT_Value)(uintptr_t)LT_String_new(encoded, encoded_length);
}

LT_DEFINE_PRIMITIVE(
    bytevector_method_as_base64_uri,
    "ByteVector>>asBase64URI",
    "(self)",
    "Return a URI-safe base64 string encoding the receiver bytes."
){
    LT_Value cursor = arguments;
    LT_ByteVector* bytevector;
    char* encoded;
    size_t encoded_length;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, bytevector, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);

    encoded = LT_base64_encode(
        LT_ByteVector_bytes(bytevector),
        LT_ByteVector_length(bytevector),
        1,
        1,
        &encoded_length
    );
    return (LT_Value)(uintptr_t)LT_String_new(encoded, encoded_length);
}

LT_DEFINE_PRIMITIVE(
    bytevector_method_as_base64_uri_with_padding,
    "ByteVector>>asBase64URIWithPadding:",
    "(self include_padding)",
    "Return a URI-safe base64 string, optionally omitting padding."
){
    LT_Value cursor = arguments;
    LT_ByteVector* bytevector;
    LT_Value include_padding;
    char* encoded;
    size_t encoded_length;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, bytevector, LT_ByteVector*, LT_ByteVector_from_value);
    LT_OBJECT_ARG(cursor, include_padding);
    LT_ARG_END(cursor);

    encoded = LT_base64_encode(
        LT_ByteVector_bytes(bytevector),
        LT_ByteVector_length(bytevector),
        1,
        boolean_from_value(include_padding, "Expected boolean padding option"),
        &encoded_length
    );
    return (LT_Value)(uintptr_t)LT_String_new(encoded, encoded_length);
}

LT_DEFINE_PRIMITIVE(
    bytevector_method_as_list,
    "ByteVector>>asList",
    "(self)",
    "Return bytevector bytes as a list of unsigned fixnums."
){
    LT_Value cursor = arguments;
    LT_ByteVector* bytevector;
    LT_ListBuilder* builder;
    size_t i;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, bytevector, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);

    builder = LT_ListBuilder_new();
    for (i = 0; i < LT_ByteVector_length(bytevector); i++){
        LT_ListBuilder_append(
            builder,
            LT_SmallInteger_new((int64_t)LT_ByteVector_at(bytevector, i))
        );
    }
    return LT_ListBuilder_value(builder);
}

LT_DEFINE_PRIMITIVE(
    bytevector_method_as_iterator,
    "ByteVector>>asIterator",
    "(self)",
    "Return an iterator over bytevector bytes."
){
    LT_Value cursor = arguments;
    LT_ByteVector* bytevector;
    LT_ByteVectorIterator* iterator;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, bytevector, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    if (LT_ByteVector_length(bytevector) == 0){
        return (LT_Value)(uintptr_t)LT_EmptyIterator_instance();
    }

    iterator = LT_Class_ALLOC(LT_ByteVectorIterator);
    iterator->bytevector = bytevector;
    iterator->index = 0;
    return (LT_Value)(uintptr_t)iterator;
}

static LT_Method_Descriptor ByteVectorIterator_methods[] = {
    {"this", &bytevector_iterator_method_this},
    {"hasThis?", &bytevector_iterator_method_has_this},
    {"next!", &bytevector_iterator_method_next},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor ByteVector_methods[] = {
    {"length", &bytevector_method_length},
    {"at:", &bytevector_method_at},
    {"at:put:", &bytevector_method_at_put},
    {"append:", &bytevector_method_append},
    {"from:to:", &bytevector_method_from_to},
    {"compareWith:", &bytevector_method_compare_with},
    {"<", &bytevector_method_less_than},
    {">", &bytevector_method_greater_than},
    {"<=", &bytevector_method_less_than_or_equal},
    {">=", &bytevector_method_greater_than_or_equal},
    {"asString", &bytevector_method_as_string},
    {"asBase64", &bytevector_method_as_base64},
    {"asBase64WithPadding:", &bytevector_method_as_base64_with_padding},
    {"asBase64URI", &bytevector_method_as_base64_uri},
    {"asBase64URIWithPadding:", &bytevector_method_as_base64_uri_with_padding},
    {"asList", &bytevector_method_as_list},
    {"asIterator", &bytevector_method_as_iterator},
    {"writeToFile:", &bytevector_method_write_to_file},
    {"splitLines", &bytevector_method_split_lines},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_ByteVectorIterator) {
    .superclass = &LT_Iterator_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "ByteVectorIterator",
    .documentation = "Iterator over bytevector bytes.",
    .instance_size = sizeof(LT_ByteVectorIterator),
    .debugPrintOn = ByteVectorIterator_debugPrintOn,
    .methods = ByteVectorIterator_methods,
};

static LT_Method_Descriptor ByteVector_class_methods[] = {
    {"fromFile:", &bytevector_class_method_from_file},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_ByteVector) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "ByteVector",
    .documentation = "Mutable indexed sequence of bytes.",
    .instance_size = sizeof(LT_ByteVector),
    .class_flags = LT_CLASS_FLAG_FLEXIBLE,
    .hash = ByteVector_hash,
    .equal_p = ByteVector_equal_p,
    .debugPrintOn = ByteVector_debugPrintOn,
    .methods = ByteVector_methods,
    .class_methods = ByteVector_class_methods,
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

int LT_ByteVector_compare(LT_ByteVector* left, LT_ByteVector* right){
    size_t left_length = LT_ByteVector_length(left);
    size_t right_length = LT_ByteVector_length(right);
    size_t common_length = left_length < right_length
        ? left_length
        : right_length;
    int result = memcmp(
        LT_ByteVector_bytes(left),
        LT_ByteVector_bytes(right),
        common_length
    );

    if (result != 0){
        return comparison_sign(result);
    }
    if (left_length < right_length){
        return -1;
    }
    if (left_length > right_length){
        return 1;
    }
    return 0;
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
