/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Stream.h>
#include <ListTalk/classes/Character.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/macros/method_macros.h>
#include <ListTalk/utils.h>
#include <ListTalk/utils/utf8.h>

#include <errno.h>
#include <stdint.h>
#include <string.h>

struct LT_Stream_s {
    LT_Object base;
    int closed;
};

struct LT_FileStream_s {
    LT_Stream base;
    FILE* file;
    int owns_file;
    int readable;
    int writable;
};

static int value_is_class_or_subclass(LT_Value value, LT_Class* expected){
    return LT_Value_class(value) == expected
        || LT_Value_is_instance_of(value, (LT_Value)(uintptr_t)expected);
}

static LT_Stream* stream_from_value(LT_Value value){
    if (!value_is_class_or_subclass(value, &LT_Stream_class)){
        LT_type_error(value, &LT_Stream_class);
    }
    return (LT_Stream*)LT_VALUE_POINTER_VALUE(value);
}

static int file_stream_p(LT_Stream* stream){
    return LT_FileStream_p((LT_Value)(uintptr_t)stream);
}

static LT_Stream* file_stream_from_value(LT_Value value){
    return (LT_Stream*)LT_VALUE_POINTER_VALUE(LT_FileStream_from_value(value));
}

static LT_FileStream* file_stream_from_stream(LT_Stream* stream){
    if (!file_stream_p(stream)){
        LT_error("Stream has no stdio FILE handle");
    }
    return (LT_FileStream*)stream;
}

static FILE* raw_stream_file(LT_Stream* stream){
    LT_FileStream* file_stream;

    if (stream->closed){
        LT_error("Stream is closed");
    }

    file_stream = file_stream_from_stream(stream);
    if (file_stream->file == NULL){
        LT_error("Stream FILE handle is NULL");
    }
    return file_stream->file;
}

static LT_Value stream_send0(LT_Value stream, char* selector){
    return LT_send(
        stream,
        LT_Symbol_new_in(LT_PACKAGE_KEYWORD, selector),
        LT_NIL,
        NULL
    );
}

static LT_Value stream_send1(LT_Value stream,
                             char* selector,
                             LT_Value argument){
    return LT_send(
        stream,
        LT_Symbol_new_in(LT_PACKAGE_KEYWORD, selector),
        LT_cons(argument, LT_NIL),
        NULL
    );
}

static void raw_check_readable(LT_Stream* stream){
    LT_FileStream* file_stream = file_stream_from_stream(stream);

    if (!file_stream->readable){
        LT_error("Stream is not readable");
    }
}

static void raw_check_writable(LT_Stream* stream){
    LT_FileStream* file_stream = file_stream_from_stream(stream);

    if (!file_stream->writable){
        LT_error("Stream is not writable");
    }
}

static void write_all(FILE* file, const void* buffer, size_t length){
    if (length > 0 && fwrite(buffer, 1, length, file) != length){
        LT_system_error("Stream write failed", errno);
    }
}

static void write_utf8_codepoint(FILE* file, uint32_t codepoint){
    char buffer[4];
    size_t length;

    if (!LT_Character_codepoint_is_valid(codepoint)){
        LT_error("Character code point out of range");
    }

    length = LT_utf8_encode(codepoint, buffer);
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
                LT_system_error("Stream read failed", errno);
            }
            break;
        }
    }

    return LT_ByteVector_new(bytes, length);
}

static int raw_stream_isReadable(LT_Stream* stream){
    return file_stream_from_stream(stream)->readable;
}

static int raw_stream_isWritable(LT_Stream* stream){
    return file_stream_from_stream(stream)->writable;
}

static void raw_stream_close(LT_Stream* stream){
    LT_FileStream* file_stream;
    FILE* file;
    int owns_file;

    if (stream->closed){
        return;
    }

    file_stream = file_stream_from_stream(stream);
    file = file_stream->file;
    owns_file = file_stream->owns_file;
    stream->closed = 1;
    file_stream->file = NULL;
    if (owns_file && file != NULL && fclose(file) != 0){
        LT_system_error("Stream close failed", errno);
    }
}

static void raw_stream_flush(LT_Stream* stream){
    if (fflush(raw_stream_file(stream)) != 0){
        LT_system_error("Stream flush failed", errno);
    }
}

static void raw_stream_seek(LT_Stream* stream, long offset){
    if (fseek(raw_stream_file(stream), offset, SEEK_CUR) != 0){
        LT_system_error("Stream seek failed", errno);
    }
}

static void raw_stream_seekFromEnd(LT_Stream* stream, long offset){
    if (fseek(raw_stream_file(stream), offset, SEEK_END) != 0){
        LT_system_error("Stream seek failed", errno);
    }
}

static void raw_stream_seekFromStart(LT_Stream* stream, long offset){
    if (fseek(raw_stream_file(stream), offset, SEEK_SET) != 0){
        LT_system_error("Stream seek failed", errno);
    }
}

static void raw_stream_writeByte(LT_Stream* stream, uint8_t byte){
    FILE* file;

    raw_check_writable(stream);
    file = raw_stream_file(stream);
    if (fputc((int)byte, file) == EOF){
        LT_system_error("Stream write failed", errno);
    }
}

static void raw_stream_writeCharacter(LT_Stream* stream, uint32_t codepoint){
    raw_check_writable(stream);
    write_utf8_codepoint(raw_stream_file(stream), codepoint);
}

static void raw_stream_writeByteVector(LT_Stream* stream,
                                       LT_ByteVector* bytevector){
    raw_check_writable(stream);
    write_all(
        raw_stream_file(stream),
        LT_ByteVector_bytes(bytevector),
        LT_ByteVector_length(bytevector)
    );
}

static void raw_stream_writeString(LT_Stream* stream, LT_String* string){
    raw_check_writable(stream);
    write_all(
        raw_stream_file(stream),
        LT_String_value_cstr(string),
        LT_String_byte_length(string)
    );
}

static void raw_stream_writeLn(LT_Stream* stream){
    raw_stream_writeByte(stream, (uint8_t)'\n');
}

static LT_Value raw_stream_readByte(LT_Stream* stream){
    int ch;

    raw_check_readable(stream);
    ch = fgetc(raw_stream_file(stream));
    if (ch == EOF){
        if (ferror(raw_stream_file(stream))){
            LT_system_error("Stream read failed", errno);
        }
        return LT_NIL;
    }
    return LT_SmallInteger_new(ch);
}

static LT_ByteVector* raw_stream_readBytes(LT_Stream* stream, size_t length){
    uint8_t* bytes;
    size_t count;

    raw_check_readable(stream);
    bytes = GC_MALLOC_ATOMIC(length == 0 ? 1 : length);
    count = fread(bytes, 1, length, raw_stream_file(stream));
    if (count < length && ferror(raw_stream_file(stream))){
        LT_system_error("Stream read failed", errno);
    }
    return LT_ByteVector_new(bytes, count);
}

static LT_Value raw_stream_readLine(LT_Stream* stream){
    LT_StringBuilder* builder = LT_StringBuilder_new();
    FILE* file;
    int ch;
    size_t length = 0;

    raw_check_readable(stream);
    file = raw_stream_file(stream);

    while ((ch = fgetc(file)) != EOF){
        if (ch == '\n'){
            char* line = LT_StringBuilder_value(builder);
            while (length > 0
                && line[length - 1] == '\r'){
                line[length - 1] = '\0';
                length--;
            }
            return (LT_Value)(uintptr_t)LT_String_new(
                line,
                length
            );
        }
        LT_StringBuilder_append_char(builder, (char)ch);
        length++;
    }

    if (ferror(file)){
        LT_system_error("Stream read failed", errno);
    }
    if (length == 0){
        return LT_NIL;
    }
    return (LT_Value)(uintptr_t)LT_String_new(
        LT_StringBuilder_value(builder),
        length
    );
}

static LT_String* raw_stream_readString(LT_Stream* stream){
    LT_StringBuilder* builder = LT_StringBuilder_new();
    FILE* file;
    int ch;
    int pending_cr = 0;

    raw_check_readable(stream);
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
        LT_system_error("Stream read failed", errno);
    }
    if (pending_cr){
        LT_StringBuilder_append_char(builder, '\n');
    }
    return LT_String_new(
        LT_StringBuilder_value(builder),
        LT_StringBuilder_length(builder)
    );
}

static LT_ByteVector* raw_stream_readByteVector(LT_Stream* stream){
    raw_check_readable(stream);
    return read_bytevector_to_eof(raw_stream_file(stream));
}

int LT_Stream_closed(LT_Value stream){
    if (LT_FileStream_p(stream)){
        return file_stream_from_value(stream)->closed;
    }
    return LT_Value_truthy_p(stream_send0(stream, "isClosed"));
}

int LT_Stream_isReadable(LT_Value stream){
    LT_Value result;

    if (LT_FileStream_p(stream)){
        return raw_stream_isReadable(file_stream_from_value(stream));
    }
    result = stream_send0(stream, "isReadable");
    return LT_Value_truthy_p(result);
}

int LT_Stream_isWritable(LT_Value stream){
    LT_Value result;

    if (LT_FileStream_p(stream)){
        return raw_stream_isWritable(file_stream_from_value(stream));
    }
    result = stream_send0(stream, "isWritable");
    return LT_Value_truthy_p(result);
}

void LT_Stream_close(LT_Value stream){
    if (LT_FileStream_p(stream)){
        raw_stream_close(file_stream_from_value(stream));
        return;
    }
    (void)stream_send0(stream, "close");
}

void LT_Stream_flush(LT_Value stream){
    if (LT_FileStream_p(stream)){
        raw_stream_flush(file_stream_from_value(stream));
        return;
    }
    (void)stream_send0(stream, "flush");
}

void LT_Stream_seek(LT_Value stream, long offset){
    if (LT_FileStream_p(stream)){
        raw_stream_seek(file_stream_from_value(stream), offset);
        return;
    }
    (void)stream_send1(
        stream,
        "seek:",
        LT_Number_smallinteger_from_long(offset, "Stream offset out of range")
    );
}

void LT_Stream_seekFromEnd(LT_Value stream, long offset){
    if (LT_FileStream_p(stream)){
        raw_stream_seekFromEnd(file_stream_from_value(stream), offset);
        return;
    }
    (void)stream_send1(
        stream,
        "seekFromEnd:",
        LT_Number_smallinteger_from_long(offset, "Stream offset out of range")
    );
}

void LT_Stream_seekFromStart(LT_Value stream, long offset){
    if (LT_FileStream_p(stream)){
        raw_stream_seekFromStart(file_stream_from_value(stream), offset);
        return;
    }
    (void)stream_send1(
        stream,
        "seekFromStart:",
        LT_Number_smallinteger_from_long(offset, "Stream offset out of range")
    );
}

void LT_Stream_writeByte(LT_Value stream, uint8_t byte){
    if (LT_FileStream_p(stream)){
        raw_stream_writeByte(file_stream_from_value(stream), byte);
        return;
    }
    (void)stream_send1(stream, "writeByte:", LT_SmallInteger_new(byte));
}

void LT_Stream_writeCharacter(LT_Value stream, uint32_t codepoint){
    if (LT_FileStream_p(stream)){
        raw_stream_writeCharacter(file_stream_from_value(stream), codepoint);
        return;
    }
    (void)stream_send1(
        stream,
        "writeCharacter:",
        LT_Character_new(codepoint)
    );
}

void LT_Stream_writeByteVector(LT_Value stream, LT_ByteVector* bytevector){
    if (LT_FileStream_p(stream)){
        raw_stream_writeByteVector(file_stream_from_value(stream), bytevector);
        return;
    }
    (void)stream_send1(
        stream,
        "writeByteVector:",
        (LT_Value)(uintptr_t)bytevector
    );
}

void LT_Stream_writeString(LT_Value stream, LT_String* string){
    if (LT_FileStream_p(stream)){
        raw_stream_writeString(file_stream_from_value(stream), string);
        return;
    }
    (void)stream_send1(stream, "writeString:", (LT_Value)(uintptr_t)string);
}

void LT_Stream_writeLn(LT_Value stream){
    if (LT_FileStream_p(stream)){
        raw_stream_writeLn(file_stream_from_value(stream));
        return;
    }
    (void)stream_send0(stream, "writeLn");
}

LT_Value LT_Stream_readByte(LT_Value stream){
    if (LT_FileStream_p(stream)){
        return raw_stream_readByte(file_stream_from_value(stream));
    }
    return stream_send0(stream, "readByte");
}

LT_ByteVector* LT_Stream_readBytes(LT_Value stream, size_t length){
    LT_Value result;

    if (LT_FileStream_p(stream)){
        return raw_stream_readBytes(file_stream_from_value(stream), length);
    }
    result = stream_send1(
        stream,
        "readBytes:",
        LT_Number_smallinteger_from_size(length, "Byte count out of range")
    );
    return LT_ByteVector_from_value(result);
}

LT_Value LT_Stream_readLine(LT_Value stream){
    if (LT_FileStream_p(stream)){
        return raw_stream_readLine(file_stream_from_value(stream));
    }
    return stream_send0(stream, "readLine");
}

LT_String* LT_Stream_readString(LT_Value stream){
    LT_Value result;

    if (LT_FileStream_p(stream)){
        return raw_stream_readString(file_stream_from_value(stream));
    }
    result = stream_send0(stream, "readString");
    return LT_String_from_value(result);
}

LT_ByteVector* LT_Stream_readByteVector(LT_Value stream){
    LT_Value result;

    if (LT_FileStream_p(stream)){
        return raw_stream_readByteVector(file_stream_from_value(stream));
    }
    result = stream_send0(stream, "readByteVector");
    return LT_ByteVector_from_value(result);
}

LT_DEFINE_PRIMITIVE(
    stream_method_is_closed,
    "Stream>>isClosed",
    "(self)",
    "Return true when stream is closed."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    (void)self;
    return LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    stream_method_is_readable,
    "Stream>>isReadable",
    "(self)",
    "Return true when stream supports reading."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    (void)self;
    return LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    stream_method_is_writable,
    "Stream>>isWritable",
    "(self)",
    "Return true when stream supports writing."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    (void)self;
    return LT_FALSE;
}

LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    stream_method_close,
    "Stream>>close",
    "Close stream."
)
LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    stream_method_flush,
    "Stream>>flush",
    "Flush buffered stream data."
)
LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_1(
    stream_method_seek,
    "Stream>>seek:",
    offset,
    "Seek from current stream position."
)
LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_1(
    stream_method_seek_from_end,
    "Stream>>seekFromEnd:",
    offset,
    "Seek from end of stream."
)
LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_1(
    stream_method_seek_from_start,
    "Stream>>seekFromStart:",
    offset,
    "Seek from start of stream."
)
LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    stream_method_read_byte,
    "Stream>>readByte",
    "Read one byte as an unsigned fixnum, or nil at EOF."
)
LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_1(
    stream_method_read_bytes,
    "Stream>>readBytes:",
    length,
    "Read up to length bytes."
)
LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    stream_method_read_line,
    "Stream>>readLine",
    "Read a line without its line terminator."
)
LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    stream_method_read_string,
    "Stream>>readString",
    "Read remaining stream bytes as UTF-8 text with normalized line endings."
)
LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    stream_method_read_bytevector,
    "Stream>>readByteVector",
    "Read remaining stream bytes into a bytevector."
)
LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_1(
    stream_method_write_byte,
    "Stream>>writeByte:",
    byte,
    "Write one byte."
)
LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_1(
    stream_method_write_character,
    "Stream>>writeCharacter:",
    character,
    "Write one character as UTF-8."
)
LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_1(
    stream_method_write_bytevector,
    "Stream>>writeByteVector:",
    bytevector,
    "Write bytevector bytes."
)
LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_1(
    stream_method_write_string,
    "Stream>>writeString:",
    string,
    "Write string UTF-8 bytes."
)
LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    stream_method_write_ln,
    "Stream>>writeLn",
    "Write a line feed byte."
)

LT_DEFINE_PRIMITIVE(
    filestream_method_is_closed,
    "FileStream>>isClosed",
    "(self)",
    "Return true when file stream is closed."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Stream* stream;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    stream = stream_from_value(self);
    return stream->closed ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    filestream_method_is_readable,
    "FileStream>>isReadable",
    "(self)",
    "Return true when file stream supports reading."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return raw_stream_isReadable(stream_from_value(self)) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    filestream_method_is_writable,
    "FileStream>>isWritable",
    "(self)",
    "Return true when file stream supports writing."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return raw_stream_isWritable(stream_from_value(self)) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    filestream_method_close,
    "FileStream>>close",
    "(self)",
    "Close stream."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    raw_stream_close(stream_from_value(self));
    return self;
}

LT_DEFINE_PRIMITIVE(
    filestream_method_flush,
    "FileStream>>flush",
    "(self)",
    "Flush buffered stream data."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    raw_stream_flush(stream_from_value(self));
    return self;
}

LT_DEFINE_PRIMITIVE(
    filestream_method_seek,
    "FileStream>>seek:",
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
    raw_stream_seek(
        stream_from_value(self),
        LT_Number_long_from_integer(offset, "Stream offset out of range")
    );
    return self;
}

LT_DEFINE_PRIMITIVE(
    filestream_method_seek_from_end,
    "FileStream>>seekFromEnd:",
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
    raw_stream_seekFromEnd(
        stream_from_value(self),
        LT_Number_long_from_integer(offset, "Stream offset out of range")
    );
    return self;
}

LT_DEFINE_PRIMITIVE(
    filestream_method_seek_from_start,
    "FileStream>>seekFromStart:",
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
    raw_stream_seekFromStart(
        stream_from_value(self),
        LT_Number_long_from_integer(offset, "Stream offset out of range")
    );
    return self;
}

LT_DEFINE_PRIMITIVE(
    filestream_method_read_byte,
    "FileStream>>readByte",
    "(self)",
    "Read one byte as an unsigned fixnum, or nil at EOF."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return raw_stream_readByte(stream_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    filestream_method_read_bytes,
    "FileStream>>readBytes:",
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
    return (LT_Value)(uintptr_t)raw_stream_readBytes(
        stream_from_value(self),
        LT_Number_nonnegative_size_from_integer(
            length,
            "Byte count out of range",
            "Byte count out of range"
        )
    );
}

LT_DEFINE_PRIMITIVE(
    filestream_method_read_line,
    "FileStream>>readLine",
    "(self)",
    "Read a line without its line terminator."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return raw_stream_readLine(stream_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    filestream_method_read_string,
    "FileStream>>readString",
    "(self)",
    "Read remaining stream bytes as UTF-8 text with normalized line endings."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)raw_stream_readString(stream_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    filestream_method_read_bytevector,
    "FileStream>>readByteVector",
    "(self)",
    "Read remaining stream bytes into a bytevector."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)raw_stream_readByteVector(stream_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    filestream_method_write_byte,
    "FileStream>>writeByte:",
    "(self byte)",
    "Write one byte."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value byte;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, byte);
    LT_ARG_END(cursor);

    raw_stream_writeByte(
        stream_from_value(self),
        LT_Number_uint8_from_integer(byte, "Byte value out of range")
    );
    return self;
}

LT_DEFINE_PRIMITIVE(
    filestream_method_write_character,
    "FileStream>>writeCharacter:",
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

    raw_stream_writeCharacter(
        stream_from_value(self),
        LT_Character_value(character)
    );
    return self;
}

LT_DEFINE_PRIMITIVE(
    filestream_method_write_bytevector,
    "FileStream>>writeByteVector:",
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

    raw_stream_writeByteVector(stream_from_value(self), bytevector);
    return self;
}

LT_DEFINE_PRIMITIVE(
    filestream_method_write_string,
    "FileStream>>writeString:",
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

    raw_stream_writeString(stream_from_value(self), string);
    return self;
}

LT_DEFINE_PRIMITIVE(
    filestream_method_write_ln,
    "FileStream>>writeLn",
    "(self)",
    "Write a line feed byte."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    raw_stream_writeLn(stream_from_value(self));
    return self;
}

static LT_Method_Descriptor Stream_methods[] = {
    {"isClosed", &stream_method_is_closed},
    {"isReadable", &stream_method_is_readable},
    {"isWritable", &stream_method_is_writable},
    {"flush", &stream_method_flush},
    {"close", &stream_method_close},
    {"seek:", &stream_method_seek},
    {"seekFromEnd:", &stream_method_seek_from_end},
    {"seekFromStart:", &stream_method_seek_from_start},
    {"readByte", &stream_method_read_byte},
    {"readBytes:", &stream_method_read_bytes},
    {"readLine", &stream_method_read_line},
    {"readString", &stream_method_read_string},
    {"readByteVector", &stream_method_read_bytevector},
    {"writeByte:", &stream_method_write_byte},
    {"writeCharacter:", &stream_method_write_character},
    {"writeByteVector:", &stream_method_write_bytevector},
    {"writeString:", &stream_method_write_string},
    {"writeLn", &stream_method_write_ln},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor FileStream_methods[] = {
    {"isClosed", &filestream_method_is_closed},
    {"isReadable", &filestream_method_is_readable},
    {"isWritable", &filestream_method_is_writable},
    {"flush", &filestream_method_flush},
    {"close", &filestream_method_close},
    {"seek:", &filestream_method_seek},
    {"seekFromEnd:", &filestream_method_seek_from_end},
    {"seekFromStart:", &filestream_method_seek_from_start},
    {"readByte", &filestream_method_read_byte},
    {"readBytes:", &filestream_method_read_bytes},
    {"readLine", &filestream_method_read_line},
    {"readString", &filestream_method_read_string},
    {"readByteVector", &filestream_method_read_bytevector},
    {"writeByte:", &filestream_method_write_byte},
    {"writeCharacter:", &filestream_method_write_character},
    {"writeByteVector:", &filestream_method_write_bytevector},
    {"writeString:", &filestream_method_write_string},
    {"writeLn", &filestream_method_write_ln},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Value filename_arg(LT_Value arguments){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* filename;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, filename, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);
    (void)self;
    return (LT_Value)(uintptr_t)filename;
}

#define DEFINE_FILESTREAM_CLASS_CONSTRUCTOR(c_name, selector, func)          \
LT_DEFINE_PRIMITIVE(                                                         \
    c_name,                                                                  \
    "FileStream class>>" selector,                                           \
    "(self filename)",                                                       \
    "Open a file stream."                                                    \
){                                                                           \
    LT_String* filename = (LT_String*)LT_VALUE_POINTER_VALUE(                \
        filename_arg(arguments)                                              \
    );                                                                       \
    (void)tail_call_unwind_marker;                                           \
    return (LT_Value)(uintptr_t)func((char*)LT_String_value_cstr(filename)); \
}

DEFINE_FILESTREAM_CLASS_CONSTRUCTOR(filestream_class_method_new,
                                    "new:",
                                    LT_FileStream_new)
DEFINE_FILESTREAM_CLASS_CONSTRUCTOR(filestream_class_method_new_for_input,
                                    "newForInput:",
                                    LT_FileStream_newForInput)
DEFINE_FILESTREAM_CLASS_CONSTRUCTOR(filestream_class_method_new_for_output,
                                    "newForOutput:",
                                    LT_FileStream_newForOutput)
DEFINE_FILESTREAM_CLASS_CONSTRUCTOR(filestream_class_method_new_for_appending,
                                    "newForAppending:",
                                    LT_FileStream_newForAppending)

static LT_Method_Descriptor FileStream_class_methods[] = {
    {"new:", &filestream_class_method_new},
    {"newForInput:", &filestream_class_method_new_for_input},
    {"newForOutput:", &filestream_class_method_new_for_output},
    {"newForAppending:", &filestream_class_method_new_for_appending},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Stream) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Stream",
    .instance_size = sizeof(LT_Stream),
    .class_flags = LT_CLASS_FLAG_ABSTRACT,
    .methods = Stream_methods,
};

LT_DEFINE_CLASS(LT_FileStream) {
    .superclass = &LT_Stream_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "FileStream",
    .instance_size = sizeof(LT_FileStream),
    .methods = FileStream_methods,
    .class_methods = FileStream_class_methods,
};

static LT_FileStream* new_for_filename(char* filename,
                                       char* primary_mode,
                                       char* fallback_mode,
                                       int readable,
                                       int writable){
    FILE* file = fopen(filename, primary_mode);

    if (file == NULL && fallback_mode != NULL){
        file = fopen(filename, fallback_mode);
    }
    if (file == NULL){
        LT_system_error("Could not open file", errno);
    }
    return LT_FileStream_newForOwnedFILE(file, readable, writable);
}

LT_FileStream* LT_FileStream_new(char* filename){
    return new_for_filename(filename, "r+b", "w+b", 1, 1);
}

LT_FileStream* LT_FileStream_newForInput(char* filename){
    return new_for_filename(filename, "rb", NULL, 1, 0);
}

LT_FileStream* LT_FileStream_newForOutput(char* filename){
    return new_for_filename(filename, "wb", NULL, 0, 1);
}

LT_FileStream* LT_FileStream_newForAppending(char* filename){
    return new_for_filename(filename, "ab", NULL, 0, 1);
}

static void file_stream_finalizer(void* object, void* data){
    LT_FileStream* stream = (LT_FileStream*)object;

    if (stream->owns_file && stream->file != NULL){
        fclose(stream->file);
    }
}

LT_FileStream* LT_FileStream_newForFILE(FILE* file,
                                        int owns_file,
                                        int readable,
                                        int writable){
    LT_FileStream* stream = LT_Class_ALLOC(LT_FileStream);

    stream->base.closed = 0;
    stream->file = file;
    stream->owns_file = owns_file;
    stream->readable = readable;
    stream->writable = writable;
    if (owns_file){
        GC_register_finalizer(stream, file_stream_finalizer, NULL, NULL, NULL);
    }
    return stream;
}

LT_FileStream* LT_FileStream_newForOwnedFILE(FILE* file,
                                             int readable,
                                             int writable){
    return LT_FileStream_newForFILE(file, 1, readable, writable);
}

LT_FileStream* LT_FileStream_newForBorrowedFILE(FILE* file,
                                                int readable,
                                                int writable){
    return LT_FileStream_newForFILE(file, 0, readable, writable);
}
