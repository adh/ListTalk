/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "BigInteger_internal.h"

#include <ListTalk/classes/RandomAccessFile.h>
#include <ListTalk/classes/Pathname.h>

#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/Integer.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Object.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/classes/Class.h>
#include <ListTalk/vm/error.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define RANDOM_ACCESS_COPY_BUFFER_SIZE 8192

struct LT_RandomAccessFile_s {
    LT_Object base;
    int fd;
    int writable;
};

static int random_access_file_fd(LT_RandomAccessFile* file){
    if (file->fd < 0){
        LT_error("RandomAccessFile is closed");
    }
    return file->fd;
}

static void random_access_file_check_writable(LT_RandomAccessFile* file){
    random_access_file_fd(file);
    if (!file->writable){
        LT_error("RandomAccessFile is not writable");
    }
}

static size_t random_access_file_length(LT_RandomAccessFile* file){
    struct stat stat_buffer;

    if (fstat(random_access_file_fd(file), &stat_buffer) != 0){
        LT_system_error("Could not stat random access file", errno);
    }
    if (stat_buffer.st_size < 0
            || (uintmax_t)stat_buffer.st_size > (uintmax_t)SIZE_MAX){
        LT_error("RandomAccessFile length out of supported range");
    }
    return (size_t)stat_buffer.st_size;
}

static void random_access_file_check_read_range(LT_RandomAccessFile* file,
                                                size_t index,
                                                size_t length){
    size_t file_length = random_access_file_length(file);

    if (index > file_length || length > file_length - index){
        LT_error("RandomAccessFile index out of bounds");
    }
}

static void random_access_file_read(LT_RandomAccessFile* file,
                                    size_t index,
                                    uint8_t* bytes,
                                    size_t length){
    size_t offset = 0;
    int fd;

    random_access_file_check_read_range(file, index, length);
    fd = random_access_file_fd(file);
    while (offset < length){
        ssize_t count = pread(
            fd,
            bytes + offset,
            length - offset,
            (off_t)(index + offset)
        );

        if (count < 0){
            if (errno == EINTR){
                continue;
            }
            LT_system_error("Random access file read failed", errno);
        }
        if (count == 0){
            LT_error("RandomAccessFile index out of bounds");
        }
        offset += (size_t)count;
    }
}

static void random_access_file_write(LT_RandomAccessFile* file,
                                     size_t index,
                                     const uint8_t* bytes,
                                     size_t length){
    size_t offset = 0;
    int fd;

    random_access_file_check_writable(file);
    fd = random_access_file_fd(file);
    while (offset < length){
        ssize_t count = pwrite(
            fd,
            bytes + offset,
            length - offset,
            (off_t)(index + offset)
        );

        if (count < 0){
            if (errno == EINTR){
                continue;
            }
            LT_system_error("Random access file write failed", errno);
        }
        if (count == 0){
            LT_error("Random access file write made no progress");
        }
        offset += (size_t)count;
    }
}

static void random_access_file_close(LT_RandomAccessFile* file){
    int fd;

    if (file->fd < 0){
        return;
    }
    fd = file->fd;
    file->fd = -1;
    if (close(fd) != 0){
        LT_system_error("Could not close random access file", errno);
    }
}

static void random_access_file_finalizer(void* object, void* data){
    LT_RandomAccessFile* file = object;
    (void)data;

    if (file->fd >= 0){
        close(file->fd);
        file->fd = -1;
    }
}

static LT_RandomAccessFile* random_access_file_new(LT_String* filename,
                                                   int writable){
    const char* path = LT_String_value_cstr(filename);
    int flags = writable ? O_RDWR | O_CREAT : O_RDONLY;
    int fd = open(path, flags, 0666);
    LT_RandomAccessFile* file;

    if (fd < 0){
        LT_system_error("Could not open random access file", errno);
    }
    file = LT_Class_ALLOC(LT_RandomAccessFile);
    file->fd = fd;
    file->writable = writable;
    GC_register_finalizer(file, random_access_file_finalizer, NULL, NULL, NULL);
    return file;
}

LT_RandomAccessFile* LT_RandomAccessFile_forReading(LT_String* filename){
    return random_access_file_new(filename, 0);
}

LT_RandomAccessFile* LT_RandomAccessFile_forWriting(LT_String* filename){
    return random_access_file_new(filename, 1);
}

static size_t random_access_file_index(LT_Value value){
    return LT_Number_nonnegative_size_from_integer(
        value,
        "RandomAccessFile index out of bounds",
        "RandomAccessFile index out of bounds"
    );
}

static LT_Value random_access_file_constructor(LT_Value arguments,
                                               int writable){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* filename;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, filename, LT_String*, LT_Pathname_like_as_string);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_RandomAccessFile_class){
        LT_error(
            writable
                ? "forWriting: is only supported on RandomAccessFile"
                : "forReading: is only supported on RandomAccessFile"
        );
    }
    return (LT_Value)(uintptr_t)(writable
        ? LT_RandomAccessFile_forWriting(filename)
        : LT_RandomAccessFile_forReading(filename));
}

LT_DEFINE_PRIMITIVE(
    random_access_file_class_method_for_reading,
    "RandomAccessFile class>>forReading:",
    "(self filename)",
    "Open an existing file for positional reads."
){
    (void)tail_call_unwind_marker;
    return random_access_file_constructor(arguments, 0);
}

LT_DEFINE_PRIMITIVE(
    random_access_file_class_method_for_writing,
    "RandomAccessFile class>>forWriting:",
    "(self filename)",
    "Open or create a file for positional reads and writes."
){
    (void)tail_call_unwind_marker;
    return random_access_file_constructor(arguments, 1);
}

LT_DEFINE_PRIMITIVE(
    random_access_file_method_length,
    "RandomAccessFile>>length",
    "(self)",
    "Return the current file length."
){
    LT_Value cursor = arguments;
    LT_RandomAccessFile* file;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, file, LT_RandomAccessFile*, LT_RandomAccessFile_from_value);
    LT_ARG_END(cursor);
    return LT_Integer_from_uintmax(random_access_file_length(file));
}

LT_DEFINE_PRIMITIVE(
    random_access_file_method_at,
    "RandomAccessFile>>at:",
    "(self index)",
    "Return the byte at index."
){
    LT_Value cursor = arguments;
    LT_RandomAccessFile* file;
    LT_Value index;
    uint8_t byte;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, file, LT_RandomAccessFile*, LT_RandomAccessFile_from_value);
    LT_OBJECT_ARG(cursor, index);
    LT_ARG_END(cursor);
    random_access_file_read(file, random_access_file_index(index), &byte, 1);
    return LT_SmallInteger_new((int64_t)byte);
}

LT_DEFINE_PRIMITIVE(
    random_access_file_method_at_put,
    "RandomAccessFile>>at:put:",
    "(self index value)",
    "Write a byte or bytevector at index, extending the file if needed."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value index;
    LT_Value value;
    LT_RandomAccessFile* file;
    size_t index_value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, index);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    file = LT_RandomAccessFile_from_value(self);
    index_value = random_access_file_index(index);
    if (LT_ByteVector_p(value)){
        LT_ByteVector* bytevector = LT_ByteVector_from_value(value);

        random_access_file_write(
            file,
            index_value,
            LT_ByteVector_bytes(bytevector),
            LT_ByteVector_length(bytevector)
        );
    } else {
        uint8_t byte = LT_Number_uint8_from_integer(
            value,
            "Byte value out of range"
        );

        random_access_file_write(file, index_value, &byte, 1);
    }
    return value;
}

LT_DEFINE_PRIMITIVE(
    random_access_file_method_from_to,
    "RandomAccessFile>>from:to:",
    "(self from to)",
    "Return bytes in the half-open file range."
){
    LT_Value cursor = arguments;
    LT_RandomAccessFile* file;
    LT_Value from;
    LT_Value to;
    size_t from_value;
    size_t to_value;
    size_t length;
    uint8_t* bytes;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, file, LT_RandomAccessFile*, LT_RandomAccessFile_from_value);
    LT_OBJECT_ARG(cursor, from);
    LT_OBJECT_ARG(cursor, to);
    LT_ARG_END(cursor);
    from_value = random_access_file_index(from);
    to_value = random_access_file_index(to);
    if (from_value > to_value){
        LT_error("RandomAccessFile index out of bounds");
    }
    length = to_value - from_value;
    random_access_file_check_read_range(file, from_value, length);
    bytes = GC_MALLOC_ATOMIC(length == 0 ? 1 : length);
    random_access_file_read(file, from_value, bytes, length);
    return (LT_Value)(uintptr_t)LT_ByteVector_new(bytes, length);
}

LT_DEFINE_PRIMITIVE(
    random_access_file_method_copy_from_length_to,
    "RandomAccessFile>>copyFrom:length:to:",
    "(self from length to)",
    "Copy a file range in place, extending the file if needed."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value from;
    LT_Value length;
    LT_Value to;
    LT_RandomAccessFile* file;
    size_t from_value;
    size_t length_value;
    size_t to_value;
    uint8_t* buffer;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, from);
    LT_OBJECT_ARG(cursor, length);
    LT_OBJECT_ARG(cursor, to);
    LT_ARG_END(cursor);
    file = LT_RandomAccessFile_from_value(self);
    from_value = random_access_file_index(from);
    length_value = random_access_file_index(length);
    to_value = random_access_file_index(to);
    random_access_file_check_writable(file);
    random_access_file_check_read_range(file, from_value, length_value);
    buffer = GC_MALLOC_ATOMIC(RANDOM_ACCESS_COPY_BUFFER_SIZE);

    if (to_value > from_value && to_value - from_value < length_value){
        size_t remaining = length_value;

        while (remaining > 0){
            size_t count = remaining < RANDOM_ACCESS_COPY_BUFFER_SIZE
                ? remaining
                : RANDOM_ACCESS_COPY_BUFFER_SIZE;
            size_t offset = remaining - count;

            random_access_file_read(file, from_value + offset, buffer, count);
            random_access_file_write(file, to_value + offset, buffer, count);
            remaining = offset;
        }
    } else {
        size_t offset = 0;

        while (offset < length_value){
            size_t count = length_value - offset;

            if (count > RANDOM_ACCESS_COPY_BUFFER_SIZE){
                count = RANDOM_ACCESS_COPY_BUFFER_SIZE;
            }
            random_access_file_read(file, from_value + offset, buffer, count);
            random_access_file_write(file, to_value + offset, buffer, count);
            offset += count;
        }
    }
    return self;
}

LT_DEFINE_PRIMITIVE(
    random_access_file_method_truncate_to,
    "RandomAccessFile>>truncateTo:",
    "(self length)",
    "Set the file length and return the receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value length;
    LT_RandomAccessFile* file;
    size_t length_value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, length);
    LT_ARG_END(cursor);
    file = LT_RandomAccessFile_from_value(self);
    length_value = random_access_file_index(length);
    random_access_file_check_writable(file);
    if (ftruncate(random_access_file_fd(file), (off_t)length_value) != 0){
        LT_system_error("Could not truncate random access file", errno);
    }
    return self;
}

LT_DEFINE_PRIMITIVE(
    random_access_file_method_close,
    "RandomAccessFile>>close!",
    "(self)",
    "Close the file descriptor and return the receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    random_access_file_close(LT_RandomAccessFile_from_value(self));
    return self;
}

static uint64_t random_access_decode(const uint8_t* bytes,
                                     size_t width,
                                     int big_endian){
    uint64_t result = 0;
    size_t i;

    for (i = 0; i < width; i++){
        size_t index = big_endian ? i : width - i - 1;

        result = (result << 8) | bytes[index];
    }
    return result;
}

static int64_t random_access_decode_signed(uint64_t value, size_t width){
    unsigned int bits = (unsigned int)(width * 8);
    uint64_t sign_bit = UINT64_C(1) << (bits - 1);

    if ((value & sign_bit) == 0){
        return (int64_t)value;
    }
    if (width == sizeof(uint64_t)){
        uint64_t magnitude = (~value) + 1;

        return magnitude == (uint64_t)INT64_MAX + 1
            ? INT64_MIN
            : -(int64_t)magnitude;
    }
    return -(int64_t)((UINT64_C(1) << bits) - value);
}

static void random_access_encode(uint8_t* bytes,
                                 size_t width,
                                 int big_endian,
                                 uint64_t value){
    size_t i;

    for (i = 0; i < width; i++){
        size_t index = big_endian ? width - i - 1 : i;

        bytes[index] = (uint8_t)value;
        value >>= 8;
    }
}

static LT_Value random_access_integer_at(LT_Value arguments,
                                         size_t width,
                                         int big_endian,
                                         int signed_p){
    LT_Value cursor = arguments;
    LT_RandomAccessFile* file;
    LT_Value index;
    uint8_t bytes[8];
    uint64_t value;

    LT_GENERIC_ARG(cursor, file, LT_RandomAccessFile*, LT_RandomAccessFile_from_value);
    LT_OBJECT_ARG(cursor, index);
    LT_ARG_END(cursor);
    random_access_file_read(file, random_access_file_index(index), bytes, width);
    value = random_access_decode(bytes, width, big_endian);
    return signed_p
        ? LT_Integer_from_intmax(random_access_decode_signed(value, width))
        : LT_Integer_from_uintmax(value);
}

static LT_Value random_access_integer_at_put(LT_Value arguments,
                                             size_t width,
                                             int big_endian,
                                             int signed_p){
    LT_Value cursor = arguments;
    LT_RandomAccessFile* file;
    LT_Value index;
    LT_Value value;
    uint8_t bytes[8];
    uint64_t encoded;

    LT_GENERIC_ARG(cursor, file, LT_RandomAccessFile*, LT_RandomAccessFile_from_value);
    LT_OBJECT_ARG(cursor, index);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    if (!LT_Integer_value_p(value)){
        LT_type_error(value, &LT_Integer_class);
    }
    if (signed_p){
        int64_t signed_value;
        int64_t min_value = width == sizeof(int64_t)
            ? INT64_MIN
            : -(INT64_C(1) << (width * 8 - 1));
        int64_t max_value = width == sizeof(int64_t)
            ? INT64_MAX
            : (INT64_C(1) << (width * 8 - 1)) - 1;

        if (!LT_Integer_to_int64(value, &signed_value)
                || signed_value < min_value || signed_value > max_value){
            LT_error("Integer does not fit requested byte width");
        }
        encoded = (uint64_t)signed_value;
    } else {
        uint64_t max_value = width == sizeof(uint64_t)
            ? UINT64_MAX
            : (UINT64_C(1) << (width * 8)) - 1;

        if (!LT_Integer_to_uint64(value, &encoded) || encoded > max_value){
            LT_error("Integer does not fit requested byte width");
        }
    }
    random_access_encode(bytes, width, big_endian, encoded);
    random_access_file_write(file, random_access_file_index(index), bytes, width);
    return value;
}

#define LT_DEFINE_RANDOM_ACCESS_INTEGER_METHODS(                            \
    c_name, selector, width, big_endian, signed_p                           \
)                                                                          \
LT_DEFINE_PRIMITIVE(                                                        \
    random_access_file_method_##c_name##_at,                                \
    "RandomAccessFile>>" selector "at:",                                  \
    "(self index)",                                                        \
    "Read a fixed-width integer at index."                                 \
){                                                                         \
    (void)tail_call_unwind_marker;                                          \
    return random_access_integer_at(arguments, width, big_endian, signed_p);\
}                                                                          \
LT_DEFINE_PRIMITIVE(                                                        \
    random_access_file_method_##c_name##_at_put,                            \
    "RandomAccessFile>>" selector "at:put:",                              \
    "(self index value)",                                                  \
    "Write a fixed-width integer at index and return it."                  \
){                                                                         \
    (void)tail_call_unwind_marker;                                          \
    return random_access_integer_at_put(                                    \
        arguments, width, big_endian, signed_p                              \
    );                                                                      \
}

LT_DEFINE_RANDOM_ACCESS_INTEGER_METHODS(beu16, "BEU16", 2, 1, 0)
LT_DEFINE_RANDOM_ACCESS_INTEGER_METHODS(beu32, "BEU32", 4, 1, 0)
LT_DEFINE_RANDOM_ACCESS_INTEGER_METHODS(beu64, "BEU64", 8, 1, 0)
LT_DEFINE_RANDOM_ACCESS_INTEGER_METHODS(bes16, "BES16", 2, 1, 1)
LT_DEFINE_RANDOM_ACCESS_INTEGER_METHODS(bes32, "BES32", 4, 1, 1)
LT_DEFINE_RANDOM_ACCESS_INTEGER_METHODS(bes64, "BES64", 8, 1, 1)
LT_DEFINE_RANDOM_ACCESS_INTEGER_METHODS(leu16, "LEU16", 2, 0, 0)
LT_DEFINE_RANDOM_ACCESS_INTEGER_METHODS(leu32, "LEU32", 4, 0, 0)
LT_DEFINE_RANDOM_ACCESS_INTEGER_METHODS(leu64, "LEU64", 8, 0, 0)
LT_DEFINE_RANDOM_ACCESS_INTEGER_METHODS(les16, "LES16", 2, 0, 1)
LT_DEFINE_RANDOM_ACCESS_INTEGER_METHODS(les32, "LES32", 4, 0, 1)
LT_DEFINE_RANDOM_ACCESS_INTEGER_METHODS(les64, "LES64", 8, 0, 1)

#undef LT_DEFINE_RANDOM_ACCESS_INTEGER_METHODS

#define RANDOM_ACCESS_INTEGER_DESCRIPTORS(c_name, selector)                 \
    {selector "at:", &random_access_file_method_##c_name##_at},             \
    {selector "at:put:", &random_access_file_method_##c_name##_at_put}

static LT_Method_Descriptor RandomAccessFile_methods[] = {
    {"length", &random_access_file_method_length},
    {"at:", &random_access_file_method_at},
    {"at:put:", &random_access_file_method_at_put},
    {"from:to:", &random_access_file_method_from_to},
    {"copyFrom:length:to:", &random_access_file_method_copy_from_length_to},
    {"truncateTo:", &random_access_file_method_truncate_to},
    {"close!", &random_access_file_method_close},
    RANDOM_ACCESS_INTEGER_DESCRIPTORS(beu16, "BEU16"),
    RANDOM_ACCESS_INTEGER_DESCRIPTORS(beu32, "BEU32"),
    RANDOM_ACCESS_INTEGER_DESCRIPTORS(beu64, "BEU64"),
    RANDOM_ACCESS_INTEGER_DESCRIPTORS(bes16, "BES16"),
    RANDOM_ACCESS_INTEGER_DESCRIPTORS(bes32, "BES32"),
    RANDOM_ACCESS_INTEGER_DESCRIPTORS(bes64, "BES64"),
    RANDOM_ACCESS_INTEGER_DESCRIPTORS(leu16, "LEU16"),
    RANDOM_ACCESS_INTEGER_DESCRIPTORS(leu32, "LEU32"),
    RANDOM_ACCESS_INTEGER_DESCRIPTORS(leu64, "LEU64"),
    RANDOM_ACCESS_INTEGER_DESCRIPTORS(les16, "LES16"),
    RANDOM_ACCESS_INTEGER_DESCRIPTORS(les32, "LES32"),
    RANDOM_ACCESS_INTEGER_DESCRIPTORS(les64, "LES64"),
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

#undef RANDOM_ACCESS_INTEGER_DESCRIPTORS

static LT_Method_Descriptor RandomAccessFile_class_methods[] = {
    {"forReading:", &random_access_file_class_method_for_reading},
    {"forWriting:", &random_access_file_class_method_for_writing},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_RandomAccessFile) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "RandomAccessFile",
    .documentation = "File descriptor backed positional byte storage.",
    .instance_size = sizeof(LT_RandomAccessFile),
    .methods = RandomAccessFile_methods,
    .class_methods = RandomAccessFile_class_methods,
};
