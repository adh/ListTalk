/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/IOStream.h>
#include <ListTalk/classes/Character.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/utils.h>

#include <limits.h>
#include <stdint.h>
#include <string.h>

struct LT_IOStream_s {
    LT_Object base;
    int closed;
};

struct LT_InputStream_s {
    LT_IOStream base;
};

struct LT_OutputStream_s {
    LT_IOStream base;
};

struct LT_FileInputStream_s {
    LT_InputStream base;
    FILE* file;
    int owns_file;
};

struct LT_FileOutputStream_s {
    LT_OutputStream base;
    FILE* file;
    int owns_file;
};

struct LT_FileIOStream_s {
    LT_InputStream base;
    FILE* file;
    int owns_file;
};

typedef struct LT_FileHandleRef_s {
    FILE* file;
    int owns_file;
} LT_FileHandleRef;

static int value_is_class_or_subclass(LT_Value value, LT_Class* expected){
    return LT_Value_class(value) == expected
        || LT_Value_is_instance_of(value, (LT_Value)(uintptr_t)expected);
}

static LT_IOStream* iostream_from_value(LT_Value value){
    if (!value_is_class_or_subclass(value, &LT_IOStream_class)){
        LT_type_error(value, &LT_IOStream_class);
    }
    return (LT_IOStream*)LT_VALUE_POINTER_VALUE(value);
}

static LT_FileHandleRef file_handle_ref(LT_IOStream* stream){
    LT_Value value = (LT_Value)(uintptr_t)stream;
    LT_FileHandleRef ref = {NULL, 0};

    if (LT_FileInputStream_p(value)){
        LT_FileInputStream* file_stream = (LT_FileInputStream*)stream;
        ref.file = file_stream->file;
        ref.owns_file = file_stream->owns_file;
        return ref;
    }
    if (LT_FileOutputStream_p(value)){
        LT_FileOutputStream* file_stream = (LT_FileOutputStream*)stream;
        ref.file = file_stream->file;
        ref.owns_file = file_stream->owns_file;
        return ref;
    }
    if (LT_FileIOStream_p(value)){
        LT_FileIOStream* file_stream = (LT_FileIOStream*)stream;
        ref.file = file_stream->file;
        ref.owns_file = file_stream->owns_file;
        return ref;
    }

    LT_error("Stream has no stdio FILE handle");
    return ref;
}

static int try_file_handle_ref(LT_IOStream* stream, LT_FileHandleRef* ref){
    LT_Value value = (LT_Value)(uintptr_t)stream;

    if (LT_FileInputStream_p(value)){
        LT_FileInputStream* file_stream = (LT_FileInputStream*)stream;
        ref->file = file_stream->file;
        ref->owns_file = file_stream->owns_file;
        return 1;
    }
    if (LT_FileOutputStream_p(value)){
        LT_FileOutputStream* file_stream = (LT_FileOutputStream*)stream;
        ref->file = file_stream->file;
        ref->owns_file = file_stream->owns_file;
        return 1;
    }
    if (LT_FileIOStream_p(value)){
        LT_FileIOStream* file_stream = (LT_FileIOStream*)stream;
        ref->file = file_stream->file;
        ref->owns_file = file_stream->owns_file;
        return 1;
    }
    return 0;
}

static FILE* raw_stream_file(LT_IOStream* stream){
    LT_FileHandleRef ref;

    if (stream->closed){
        LT_error("Stream is closed");
    }

    ref = file_handle_ref(stream);
    if (ref.file == NULL){
        LT_error("Stream FILE handle is NULL");
    }
    return ref.file;
}

static LT_Value stream_send0(LT_IOStream* stream, char* selector){
    return LT_send(
        (LT_Value)(uintptr_t)stream,
        LT_Symbol_new_in(LT_PACKAGE_KEYWORD, selector),
        LT_NIL,
        NULL
    );
}

static LT_Value stream_send1(LT_IOStream* stream,
                             char* selector,
                             LT_Value argument){
    return LT_send(
        (LT_Value)(uintptr_t)stream,
        LT_Symbol_new_in(LT_PACKAGE_KEYWORD, selector),
        LT_cons(argument, LT_NIL),
        NULL
    );
}

static void stream_mark_file_closed(LT_IOStream* stream){
    LT_Value value = (LT_Value)(uintptr_t)stream;

    if (LT_FileInputStream_p(value)){
        ((LT_FileInputStream*)stream)->file = NULL;
    } else if (LT_FileOutputStream_p(value)){
        ((LT_FileOutputStream*)stream)->file = NULL;
    } else if (LT_FileIOStream_p(value)){
        ((LT_FileIOStream*)stream)->file = NULL;
    }
}

static void stream_check_input(LT_IOStream* stream){
    LT_Value value = (LT_Value)(uintptr_t)stream;

    if (!value_is_class_or_subclass(value, &LT_InputStream_class)){
        LT_error("Stream is not readable");
    }
}

static void stream_check_output(LT_IOStream* stream){
    LT_Value value = (LT_Value)(uintptr_t)stream;

    if (!value_is_class_or_subclass(value, &LT_OutputStream_class)
        && !LT_FileIOStream_p(value)){
        LT_error("Stream is not writable");
    }
}

static size_t size_from_fixnum(LT_Value value, char* what){
    int64_t fixnum = LT_SmallInteger_value(value);

    if (fixnum < 0){
        LT_error(what);
    }
    return (size_t)fixnum;
}

static long long_from_fixnum(LT_Value value){
    int64_t fixnum = LT_SmallInteger_value(value);

    if (fixnum < (int64_t)LONG_MIN || fixnum > (int64_t)LONG_MAX){
        LT_error("Stream offset out of range");
    }
    return (long)fixnum;
}

static LT_Value smallinteger_from_size(size_t value, char* what){
    if (value > (size_t)LT_VALUE_FIXNUM_MAX){
        LT_error(what);
    }
    return LT_SmallInteger_new((int64_t)value);
}

static LT_Value smallinteger_from_long(long value){
    if (value < (long)LT_VALUE_FIXNUM_MIN
        || value > (long)LT_VALUE_FIXNUM_MAX){
        LT_error("Stream offset out of range");
    }
    return LT_SmallInteger_new((int64_t)value);
}

static void check_byte_value(int64_t value){
    if (value < 0 || value > UINT8_MAX){
        LT_error("Byte value out of range");
    }
}

static void write_all(FILE* file, const void* buffer, size_t length){
    if (length > 0 && fwrite(buffer, 1, length, file) != length){
        LT_error("Stream write failed");
    }
}

static void write_utf8_codepoint(FILE* file, uint32_t codepoint){
    uint8_t buffer[4];
    size_t length;

    if (!LT_Character_codepoint_is_valid(codepoint)){
        LT_error("Character code point out of range");
    }

    if (codepoint <= UINT32_C(0x7f)){
        buffer[0] = (uint8_t)codepoint;
        length = 1;
    } else if (codepoint <= UINT32_C(0x7ff)){
        buffer[0] = (uint8_t)(0xc0 | (codepoint >> 6));
        buffer[1] = (uint8_t)(0x80 | (codepoint & 0x3f));
        length = 2;
    } else if (codepoint <= UINT32_C(0xffff)){
        buffer[0] = (uint8_t)(0xe0 | (codepoint >> 12));
        buffer[1] = (uint8_t)(0x80 | ((codepoint >> 6) & 0x3f));
        buffer[2] = (uint8_t)(0x80 | (codepoint & 0x3f));
        length = 3;
    } else {
        buffer[0] = (uint8_t)(0xf0 | (codepoint >> 18));
        buffer[1] = (uint8_t)(0x80 | ((codepoint >> 12) & 0x3f));
        buffer[2] = (uint8_t)(0x80 | ((codepoint >> 6) & 0x3f));
        buffer[3] = (uint8_t)(0x80 | (codepoint & 0x3f));
        length = 4;
    }

    write_all(file, buffer, length);
}

static LT_ByteVector* read_bytevector_to_eof(FILE* file){
    size_t capacity = 256;
    size_t length = 0;
    uint8_t* bytes = GC_MALLOC_ATOMIC(capacity);

    while (1){
        size_t available = capacity - length;
        size_t count;

        if (available == 0){
            size_t new_capacity = capacity * 2;
            uint8_t* new_bytes = GC_MALLOC_ATOMIC(new_capacity);
            memcpy(new_bytes, bytes, length);
            bytes = new_bytes;
            capacity = new_capacity;
            available = capacity - length;
        }

        count = fread(bytes + length, 1, available, file);
        length += count;
        if (count < available){
            if (ferror(file)){
                LT_error("Stream read failed");
            }
            break;
        }
    }

    return LT_ByteVector_new(bytes, length);
}

static void raw_iostream_close(LT_IOStream* stream){
    LT_FileHandleRef ref;

    if (stream->closed){
        return;
    }

    ref = file_handle_ref(stream);
    stream->closed = 1;
    stream_mark_file_closed(stream);
    if (ref.owns_file && ref.file != NULL && fclose(ref.file) != 0){
        LT_error("Stream close failed");
    }
}

static void raw_iostream_flush(LT_IOStream* stream){
    FILE* file = raw_stream_file(stream);

    if (fflush(file) != 0){
        LT_error("Stream flush failed");
    }
}

static void raw_iostream_seek(LT_IOStream* stream, long offset){
    if (fseek(raw_stream_file(stream), offset, SEEK_CUR) != 0){
        LT_error("Stream seek failed");
    }
}

static void raw_iostream_seekFromEnd(LT_IOStream* stream, long offset){
    if (fseek(raw_stream_file(stream), offset, SEEK_END) != 0){
        LT_error("Stream seek failed");
    }
}

static void raw_iostream_seekFromStart(LT_IOStream* stream, long offset){
    if (fseek(raw_stream_file(stream), offset, SEEK_SET) != 0){
        LT_error("Stream seek failed");
    }
}

static void raw_outputstream_writeByte(LT_IOStream* stream, uint8_t byte){
    FILE* file;

    stream_check_output(stream);
    file = raw_stream_file(stream);
    if (fputc((int)byte, file) == EOF){
        LT_error("Stream write failed");
    }
}

static void raw_outputstream_writeCharacter(LT_IOStream* stream,
                                            uint32_t codepoint){
    stream_check_output(stream);
    write_utf8_codepoint(raw_stream_file(stream), codepoint);
}

static void raw_outputstream_writeByteVector(LT_IOStream* stream,
                                             LT_ByteVector* bytevector){
    stream_check_output(stream);
    write_all(
        raw_stream_file(stream),
        LT_ByteVector_bytes(bytevector),
        LT_ByteVector_length(bytevector)
    );
}

static void raw_outputstream_writeString(LT_IOStream* stream,
                                         LT_String* string){
    stream_check_output(stream);
    write_all(
        raw_stream_file(stream),
        LT_String_value_cstr(string),
        LT_String_byte_length(string)
    );
}

static void raw_outputstream_writeLn(LT_IOStream* stream){
    raw_outputstream_writeByte(stream, (uint8_t)'\n');
}

static LT_Value raw_inputstream_readByte(LT_IOStream* stream){
    int ch;

    stream_check_input(stream);
    ch = fgetc(raw_stream_file(stream));
    if (ch == EOF){
        if (ferror(raw_stream_file(stream))){
            LT_error("Stream read failed");
        }
        return LT_NIL;
    }
    return LT_SmallInteger_new(ch);
}

static LT_ByteVector* raw_inputstream_readBytes(LT_IOStream* stream,
                                                size_t length){
    uint8_t* bytes;
    size_t count;

    stream_check_input(stream);
    bytes = GC_MALLOC_ATOMIC(length == 0 ? 1 : length);
    count = fread(bytes, 1, length, raw_stream_file(stream));
    if (count < length && ferror(raw_stream_file(stream))){
        LT_error("Stream read failed");
    }
    return LT_ByteVector_new(bytes, count);
}

static LT_Value raw_inputstream_readLine(LT_IOStream* stream){
    LT_StringBuilder* builder = LT_StringBuilder_new();
    FILE* file;
    int ch;
    size_t length = 0;

    stream_check_input(stream);
    file = raw_stream_file(stream);

    while ((ch = fgetc(file)) != EOF){
        if (ch == '\n'){
            while (length > 0
                && LT_StringBuilder_value(builder)[length - 1] == '\r'){
                LT_StringBuilder_value(builder)[length - 1] = '\0';
                length--;
            }
            return (LT_Value)(uintptr_t)LT_String_new(
                LT_StringBuilder_value(builder),
                length
            );
        }
        LT_StringBuilder_append_char(builder, (char)ch);
        length++;
    }

    if (ferror(file)){
        LT_error("Stream read failed");
    }
    if (length == 0){
        return LT_NIL;
    }
    return (LT_Value)(uintptr_t)LT_String_new(
        LT_StringBuilder_value(builder),
        length
    );
}

static LT_String* raw_inputstream_readString(LT_IOStream* stream){
    LT_StringBuilder* builder = LT_StringBuilder_new();
    FILE* file;
    int ch;
    int pending_cr = 0;

    stream_check_input(stream);
    file = raw_stream_file(stream);

    while ((ch = fgetc(file)) != EOF){
        if (ch == '\r'){
            if (pending_cr){
                LT_StringBuilder_append_char(builder, '\n');
            }
            pending_cr = 1;
            continue;
        }
        if (ch == '\n'){
            LT_StringBuilder_append_char(builder, '\n');
            pending_cr = 0;
            continue;
        }
        if (pending_cr){
            LT_StringBuilder_append_char(builder, '\n');
            pending_cr = 0;
        }
        LT_StringBuilder_append_char(builder, (char)ch);
    }

    if (ferror(file)){
        LT_error("Stream read failed");
    }
    if (pending_cr){
        LT_StringBuilder_append_char(builder, '\n');
    }
    return LT_String_new(
        LT_StringBuilder_value(builder),
        LT_StringBuilder_length(builder)
    );
}

static LT_ByteVector* raw_inputstream_readByteVector(LT_IOStream* stream){
    stream_check_input(stream);
    return read_bytevector_to_eof(raw_stream_file(stream));
}

int LT_IOStream_closed(LT_IOStream* stream){
    return stream->closed;
}

void LT_IOStream_close(LT_IOStream* stream){
    LT_FileHandleRef ref;

    if (try_file_handle_ref(stream, &ref)){
        raw_iostream_close(stream);
        return;
    }
    (void)stream_send0(stream, "close");
}

void LT_IOStream_flush(LT_IOStream* stream){
    LT_FileHandleRef ref;

    if (try_file_handle_ref(stream, &ref)){
        raw_iostream_flush(stream);
        return;
    }
    (void)stream_send0(stream, "flush");
}

void LT_IOStream_seek(LT_IOStream* stream, long offset){
    LT_FileHandleRef ref;

    if (try_file_handle_ref(stream, &ref)){
        raw_iostream_seek(stream, offset);
        return;
    }
    (void)stream_send1(stream, "seek:", smallinteger_from_long(offset));
}

void LT_IOStream_seekFromEnd(LT_IOStream* stream, long offset){
    LT_FileHandleRef ref;

    if (try_file_handle_ref(stream, &ref)){
        raw_iostream_seekFromEnd(stream, offset);
        return;
    }
    (void)stream_send1(
        stream,
        "seekFromEnd:",
        smallinteger_from_long(offset)
    );
}

void LT_IOStream_seekFromStart(LT_IOStream* stream, long offset){
    LT_FileHandleRef ref;

    if (try_file_handle_ref(stream, &ref)){
        raw_iostream_seekFromStart(stream, offset);
        return;
    }
    (void)stream_send1(
        stream,
        "seekFromStart:",
        smallinteger_from_long(offset)
    );
}

void LT_OutputStream_writeByte(LT_IOStream* stream, uint8_t byte){
    LT_FileHandleRef ref;

    if (try_file_handle_ref(stream, &ref)){
        raw_outputstream_writeByte(stream, byte);
        return;
    }
    (void)stream_send1(stream, "writeByte:", LT_SmallInteger_new(byte));
}

void LT_OutputStream_writeCharacter(LT_IOStream* stream, uint32_t codepoint){
    LT_FileHandleRef ref;

    if (try_file_handle_ref(stream, &ref)){
        raw_outputstream_writeCharacter(stream, codepoint);
        return;
    }
    (void)stream_send1(
        stream,
        "writeCharacter:",
        LT_Character_new(codepoint)
    );
}

void LT_OutputStream_writeByteVector(LT_IOStream* stream,
                                     LT_ByteVector* bytevector){
    LT_FileHandleRef ref;

    if (try_file_handle_ref(stream, &ref)){
        raw_outputstream_writeByteVector(stream, bytevector);
        return;
    }
    (void)stream_send1(
        stream,
        "writeByteVector:",
        (LT_Value)(uintptr_t)bytevector
    );
}

void LT_OutputStream_writeString(LT_IOStream* stream, LT_String* string){
    LT_FileHandleRef ref;

    if (try_file_handle_ref(stream, &ref)){
        raw_outputstream_writeString(stream, string);
        return;
    }
    (void)stream_send1(stream, "writeString:", (LT_Value)(uintptr_t)string);
}

void LT_OutputStream_writeLn(LT_IOStream* stream){
    LT_FileHandleRef ref;

    if (try_file_handle_ref(stream, &ref)){
        raw_outputstream_writeLn(stream);
        return;
    }
    (void)stream_send0(stream, "writeLn");
}

LT_Value LT_InputStream_readByte(LT_IOStream* stream){
    LT_FileHandleRef ref;

    if (try_file_handle_ref(stream, &ref)){
        return raw_inputstream_readByte(stream);
    }
    return stream_send0(stream, "readByte");
}

LT_ByteVector* LT_InputStream_readBytes(LT_IOStream* stream, size_t length){
    LT_FileHandleRef ref;
    LT_Value result;

    if (try_file_handle_ref(stream, &ref)){
        return raw_inputstream_readBytes(stream, length);
    }
    result = stream_send1(
        stream,
        "readBytes:",
        smallinteger_from_size(length, "Byte count out of range")
    );
    return LT_ByteVector_from_value(result);
}

LT_Value LT_InputStream_readLine(LT_IOStream* stream){
    LT_FileHandleRef ref;

    if (try_file_handle_ref(stream, &ref)){
        return raw_inputstream_readLine(stream);
    }
    return stream_send0(stream, "readLine");
}

LT_String* LT_InputStream_readString(LT_IOStream* stream){
    LT_FileHandleRef ref;
    LT_Value result;

    if (try_file_handle_ref(stream, &ref)){
        return raw_inputstream_readString(stream);
    }
    result = stream_send0(stream, "readString");
    return LT_String_from_value(result);
}

LT_ByteVector* LT_InputStream_readByteVector(LT_IOStream* stream){
    LT_FileHandleRef ref;
    LT_Value result;

    if (try_file_handle_ref(stream, &ref)){
        return raw_inputstream_readByteVector(stream);
    }
    result = stream_send0(stream, "readByteVector");
    return LT_ByteVector_from_value(result);
}

LT_DEFINE_PRIMITIVE(
    iostream_method_close,
    "IOStream>>close",
    "(self)",
    "Close stream."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    raw_iostream_close(iostream_from_value(self));
    return self;
}

LT_DEFINE_PRIMITIVE(
    iostream_method_flush,
    "IOStream>>flush",
    "(self)",
    "Flush buffered stream data."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    raw_iostream_flush(iostream_from_value(self));
    return self;
}

LT_DEFINE_PRIMITIVE(
    iostream_method_seek,
    "IOStream>>seek:",
    "(self offset)",
    "Seek from current stream position."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value offset;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, offset);
    LT_ARG_END(cursor);
    raw_iostream_seek(iostream_from_value(self), long_from_fixnum(offset));
    return self;
}

LT_DEFINE_PRIMITIVE(
    iostream_method_seek_from_end,
    "IOStream>>seekFromEnd:",
    "(self offset)",
    "Seek from end of stream."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value offset;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, offset);
    LT_ARG_END(cursor);
    raw_iostream_seekFromEnd(iostream_from_value(self), long_from_fixnum(offset));
    return self;
}

LT_DEFINE_PRIMITIVE(
    iostream_method_seek_from_start,
    "IOStream>>seekFromStart:",
    "(self offset)",
    "Seek from start of stream."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value offset;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, offset);
    LT_ARG_END(cursor);
    raw_iostream_seekFromStart(iostream_from_value(self), long_from_fixnum(offset));
    return self;
}

LT_DEFINE_PRIMITIVE(
    inputstream_method_read_byte,
    "InputStream>>readByte",
    "(self)",
    "Read one byte as an unsigned fixnum, or nil at EOF."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return raw_inputstream_readByte(iostream_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    inputstream_method_read_bytes,
    "InputStream>>readBytes:",
    "(self length)",
    "Read up to length bytes."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value length;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, length);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)raw_inputstream_readBytes(
        iostream_from_value(self),
        size_from_fixnum(length, "Byte count out of range")
    );
}

LT_DEFINE_PRIMITIVE(
    inputstream_method_read_line,
    "InputStream>>readLine",
    "(self)",
    "Read a line without its line terminator."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return raw_inputstream_readLine(iostream_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    inputstream_method_read_string,
    "InputStream>>readString",
    "(self)",
    "Read remaining stream bytes as UTF-8 text with normalized line endings."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)raw_inputstream_readString(iostream_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    inputstream_method_read_bytevector,
    "InputStream>>readByteVector",
    "(self)",
    "Read remaining stream bytes into a bytevector."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)raw_inputstream_readByteVector(
        iostream_from_value(self)
    );
}

LT_DEFINE_PRIMITIVE(
    outputstream_method_write_byte,
    "OutputStream>>writeByte:",
    "(self byte)",
    "Write one byte."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value byte;
    int64_t byte_value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, byte);
    LT_ARG_END(cursor);

    byte_value = LT_SmallInteger_value(byte);
    check_byte_value(byte_value);
    raw_outputstream_writeByte(iostream_from_value(self), (uint8_t)byte_value);
    return self;
}

LT_DEFINE_PRIMITIVE(
    outputstream_method_write_character,
    "OutputStream>>writeCharacter:",
    "(self character)",
    "Write one character as UTF-8."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value character;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, character);
    LT_ARG_END(cursor);

    raw_outputstream_writeCharacter(
        iostream_from_value(self),
        LT_Character_value(character)
    );
    return self;
}

LT_DEFINE_PRIMITIVE(
    outputstream_method_write_bytevector,
    "OutputStream>>writeByteVector:",
    "(self bytevector)",
    "Write bytevector bytes."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_ByteVector* bytevector;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, bytevector, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);

    raw_outputstream_writeByteVector(iostream_from_value(self), bytevector);
    return self;
}

LT_DEFINE_PRIMITIVE(
    outputstream_method_write_string,
    "OutputStream>>writeString:",
    "(self string)",
    "Write string UTF-8 bytes."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* string;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, string, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);

    raw_outputstream_writeString(iostream_from_value(self), string);
    return self;
}

LT_DEFINE_PRIMITIVE(
    outputstream_method_write_ln,
    "OutputStream>>writeLn",
    "(self)",
    "Write a line feed byte."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    raw_outputstream_writeLn(iostream_from_value(self));
    return self;
}

static LT_Method_Descriptor IOStream_methods[] = {
    {"flush", &iostream_method_flush},
    {"close", &iostream_method_close},
    {"seek:", &iostream_method_seek},
    {"seekFromEnd:", &iostream_method_seek_from_end},
    {"seekFromStart:", &iostream_method_seek_from_start},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor InputStream_methods[] = {
    {"readByte", &inputstream_method_read_byte},
    {"readBytes:", &inputstream_method_read_bytes},
    {"readLine", &inputstream_method_read_line},
    {"readString", &inputstream_method_read_string},
    {"readByteVector", &inputstream_method_read_bytevector},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor OutputStream_methods[] = {
    {"writeByte:", &outputstream_method_write_byte},
    {"writeCharacter:", &outputstream_method_write_character},
    {"writeByteVector:", &outputstream_method_write_bytevector},
    {"writeString:", &outputstream_method_write_string},
    {"writeLn", &outputstream_method_write_ln},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor FileIOStream_methods[] = {
    {"writeByte:", &outputstream_method_write_byte},
    {"writeCharacter:", &outputstream_method_write_character},
    {"writeByteVector:", &outputstream_method_write_bytevector},
    {"writeString:", &outputstream_method_write_string},
    {"writeLn", &outputstream_method_write_ln},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_PRIMITIVE(
    fileinputstream_class_method_new_for_filename,
    "FileInputStream class>>newForFilename:",
    "(self filename)",
    "Open a file input stream for filename."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* filename;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, filename, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);
    (void)self;
    return (LT_Value)(uintptr_t)LT_FileInputStream_newForFilename(
        (char*)LT_String_value_cstr(filename)
    );
}

LT_DEFINE_PRIMITIVE(
    fileoutputstream_class_method_new_for_filename,
    "FileOutputStream class>>newForFilename:",
    "(self filename)",
    "Open a file output stream for filename."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* filename;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, filename, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);
    (void)self;
    return (LT_Value)(uintptr_t)LT_FileOutputStream_newForFilename(
        (char*)LT_String_value_cstr(filename)
    );
}

LT_DEFINE_PRIMITIVE(
    fileiostream_class_method_new_for_filename,
    "FileIOStream class>>newForFilename:",
    "(self filename)",
    "Open a read/write file stream for filename."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* filename;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, filename, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);
    (void)self;
    return (LT_Value)(uintptr_t)LT_FileIOStream_newForFilename(
        (char*)LT_String_value_cstr(filename)
    );
}

static LT_Method_Descriptor FileInputStream_class_methods[] = {
    {"newForFilename:", &fileinputstream_class_method_new_for_filename},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor FileOutputStream_class_methods[] = {
    {"newForFilename:", &fileoutputstream_class_method_new_for_filename},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor FileIOStream_class_methods[] = {
    {"newForFilename:", &fileiostream_class_method_new_for_filename},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_IOStream) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "IOStream",
    .instance_size = sizeof(LT_IOStream),
    .class_flags = LT_CLASS_FLAG_ABSTRACT,
    .methods = IOStream_methods,
};

LT_DEFINE_CLASS(LT_InputStream) {
    .superclass = &LT_IOStream_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "InputStream",
    .instance_size = sizeof(LT_InputStream),
    .class_flags = LT_CLASS_FLAG_ABSTRACT,
    .methods = InputStream_methods,
};

LT_DEFINE_CLASS(LT_OutputStream) {
    .superclass = &LT_IOStream_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "OutputStream",
    .instance_size = sizeof(LT_OutputStream),
    .class_flags = LT_CLASS_FLAG_ABSTRACT,
    .methods = OutputStream_methods,
};

LT_DEFINE_CLASS(LT_FileInputStream) {
    .superclass = &LT_InputStream_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "FileInputStream",
    .instance_size = sizeof(LT_FileInputStream),
    .class_methods = FileInputStream_class_methods,
};

LT_DEFINE_CLASS(LT_FileOutputStream) {
    .superclass = &LT_OutputStream_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "FileOutputStream",
    .instance_size = sizeof(LT_FileOutputStream),
    .class_methods = FileOutputStream_class_methods,
};

LT_DEFINE_CLASS(LT_FileIOStream) {
    .superclass = &LT_InputStream_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "FileIOStream",
    .instance_size = sizeof(LT_FileIOStream),
    .methods = FileIOStream_methods,
    .class_methods = FileIOStream_class_methods,
};

LT_FileInputStream* LT_FileInputStream_newForFilename(char* filename){
    FILE* file = fopen(filename, "rb");

    if (file == NULL){
        LT_error("Could not open file for reading");
    }
    return LT_FileInputStream_newForOwnedFILE(file);
}

LT_FileInputStream* LT_FileInputStream_newForFILE(FILE* file, int owns_file){
    LT_FileInputStream* stream = LT_Class_ALLOC(LT_FileInputStream);

    stream->base.base.closed = 0;
    stream->file = file;
    stream->owns_file = owns_file;
    return stream;
}

LT_FileInputStream* LT_FileInputStream_newForOwnedFILE(FILE* file){
    return LT_FileInputStream_newForFILE(file, 1);
}

LT_FileInputStream* LT_FileInputStream_newForBorrowedFILE(FILE* file){
    return LT_FileInputStream_newForFILE(file, 0);
}

LT_FileOutputStream* LT_FileOutputStream_newForFilename(char* filename){
    FILE* file = fopen(filename, "wb");

    if (file == NULL){
        LT_error("Could not open file for writing");
    }
    return LT_FileOutputStream_newForOwnedFILE(file);
}

LT_FileOutputStream* LT_FileOutputStream_newForFILE(FILE* file, int owns_file){
    LT_FileOutputStream* stream = LT_Class_ALLOC(LT_FileOutputStream);

    stream->base.base.closed = 0;
    stream->file = file;
    stream->owns_file = owns_file;
    return stream;
}

LT_FileOutputStream* LT_FileOutputStream_newForOwnedFILE(FILE* file){
    return LT_FileOutputStream_newForFILE(file, 1);
}

LT_FileOutputStream* LT_FileOutputStream_newForBorrowedFILE(FILE* file){
    return LT_FileOutputStream_newForFILE(file, 0);
}

LT_FileIOStream* LT_FileIOStream_newForFilename(char* filename){
    FILE* file = fopen(filename, "r+b");

    if (file == NULL){
        file = fopen(filename, "w+b");
    }
    if (file == NULL){
        LT_error("Could not open file for reading and writing");
    }
    return LT_FileIOStream_newForOwnedFILE(file);
}

LT_FileIOStream* LT_FileIOStream_newForFILE(FILE* file, int owns_file){
    LT_FileIOStream* stream = LT_Class_ALLOC(LT_FileIOStream);

    stream->base.base.closed = 0;
    stream->file = file;
    stream->owns_file = owns_file;
    return stream;
}

LT_FileIOStream* LT_FileIOStream_newForOwnedFILE(FILE* file){
    return LT_FileIOStream_newForFILE(file, 1);
}

LT_FileIOStream* LT_FileIOStream_newForBorrowedFILE(FILE* file){
    return LT_FileIOStream_newForFILE(file, 0);
}
