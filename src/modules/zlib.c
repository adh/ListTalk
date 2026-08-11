/*
 * SPDX-License-Identifier: MIT
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/Character.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Stream.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/utils.h>
#include <ListTalk/utils/utf8.h>

#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <zlib.h>

LT_DECLARE_CLASS(LT_GzipStream);

struct LT_GzipStream_s {
    LT_Object base;
    int closed;
    gzFile file;
    int readable;
    int writable;
    LT_String* path;
};

static void gzip_stream_error(LT_GzipStream* stream, const char* operation){
    int status = Z_OK;
    const char* detail = gzerror(stream->file, &status);

    if (detail == NULL || detail[0] == '\0'){
        detail = zError(status);
    }
    LT_error(LT_sprintf("gzip %s failed: %s", operation, detail));
}

static void gzip_stream_check_open(LT_GzipStream* stream){
    if (stream->closed || stream->file == NULL){
        LT_error("Gzip stream is closed");
    }
}

static void gzip_stream_check_readable(LT_GzipStream* stream){
    gzip_stream_check_open(stream);
    if (!stream->readable){
        LT_error("Gzip stream is not readable");
    }
}

static void gzip_stream_check_writable(LT_GzipStream* stream){
    gzip_stream_check_open(stream);
    if (!stream->writable){
        LT_error("Gzip stream is not writable");
    }
}

static LT_GzipStream* gzip_stream_open(LT_String* path, const char* mode){
    LT_GzipStream* stream = LT_Class_ALLOC(LT_GzipStream);

    stream->closed = 0;
    stream->file = gzopen(LT_String_value_cstr(path), mode);
    stream->readable = mode[0] == 'r';
    stream->writable = !stream->readable;
    stream->path = path;
    if (stream->file == NULL){
        LT_error(LT_sprintf(
            "Could not open gzip file: %s",
            LT_String_value_cstr(path)
        ));
    }
    return stream;
}

static void gzip_stream_finalizer(void* object, void* data){
    LT_GzipStream* stream = object;

    (void)data;
    if (!stream->closed && stream->file != NULL){
        gzclose(stream->file);
    }
}

static LT_Value gzip_stream_new(LT_String* path, const char* mode){
    LT_GzipStream* stream = gzip_stream_open(path, mode);

    GC_register_finalizer(stream, gzip_stream_finalizer, NULL, NULL, NULL);
    return (LT_Value)(uintptr_t)stream;
}

static size_t gzip_stream_read(LT_GzipStream* stream,
                               uint8_t* bytes,
                               size_t length){
    size_t total = 0;

    gzip_stream_check_readable(stream);
    while (total < length){
        size_t remaining = length - total;
        unsigned int chunk = remaining > INT_MAX ? INT_MAX : (unsigned int)remaining;
        int count = gzread(stream->file, bytes + total, chunk);

        if (count < 0){
            gzip_stream_error(stream, "read");
        }
        if (count == 0){
            break;
        }
        total += (size_t)count;
    }
    return total;
}

static void gzip_stream_write(LT_GzipStream* stream,
                              const uint8_t* bytes,
                              size_t length){
    size_t total = 0;

    gzip_stream_check_writable(stream);
    while (total < length){
        size_t remaining = length - total;
        unsigned int chunk = remaining > INT_MAX ? INT_MAX : (unsigned int)remaining;
        int count = gzwrite(stream->file, bytes + total, chunk);

        if (count <= 0){
            gzip_stream_error(stream, "write");
        }
        total += (size_t)count;
    }
}

static void bind_zlib_primitive(LT_Environment* environment,
                                LT_Package* package,
                                LT_Primitive* primitive){
    LT_Environment_bind(
        environment,
        LT_Symbol_new_in(package, primitive->name),
        LT_Primitive_from_static(primitive),
        LT_ENV_BINDING_FLAG_CONSTANT
    );
}

static void bind_zlib_constant(LT_Environment* environment,
                               LT_Package* package,
                               const char* name,
                               LT_Value value){
    LT_Environment_bind(
        environment,
        LT_Symbol_new_in(package, (char*)name),
        value,
        LT_ENV_BINDING_FLAG_CONSTANT
    );
}

static void zlib_error(const char* operation, int status){
    const char* detail = zError(status);

    if (detail == NULL){
        detail = "unknown zlib error";
    }
    LT_error(LT_sprintf("zlib %s failed: %s", operation, detail));
}

static LT_Value zlib_compress(LT_ByteVector* input, int level){
    size_t input_length = LT_ByteVector_length(input);
    uLong source_length = (uLong)input_length;
    uLongf output_length;
    uint8_t* output;
    int status;

    if ((size_t)source_length != input_length){
        LT_error("Input is too large for zlib");
    }

    output_length = compressBound(source_length);
    if ((uLong)(size_t)output_length != output_length){
        LT_error("Compressed output is too large for ListTalk");
    }
    output = GC_MALLOC_ATOMIC(output_length == 0 ? 1 : (size_t)output_length);
    status = compress2(
        output,
        &output_length,
        LT_ByteVector_bytes(input),
        source_length,
        level
    );
    if (status != Z_OK){
        zlib_error("compression", status);
    }
    return (LT_Value)(uintptr_t)LT_ByteVector_new(
        output,
        (size_t)output_length
    );
}

static LT_Value zlib_uncompress(LT_ByteVector* input){
    const uint8_t* input_bytes = LT_ByteVector_bytes(input);
    size_t input_length = LT_ByteVector_length(input);
    size_t input_offset = 0;
    LT_StringBuilder* output = LT_StringBuilder_new();
    z_stream stream;
    int status;

    memset(&stream, 0, sizeof(stream));
    status = inflateInit(&stream);
    if (status != Z_OK){
        zlib_error("decompression initialization", status);
    }

    while (1){
        uint8_t buffer[8192];
        size_t produced;

        if (stream.avail_in == 0 && input_offset < input_length){
            size_t remaining = input_length - input_offset;
            uInt chunk = remaining > UINT_MAX ? UINT_MAX : (uInt)remaining;

            stream.next_in = (Bytef*)(input_bytes + input_offset);
            stream.avail_in = chunk;
            input_offset += chunk;
        }

        stream.next_out = buffer;
        stream.avail_out = (uInt)sizeof(buffer);
        status = inflate(&stream, Z_NO_FLUSH);
        produced = sizeof(buffer) - stream.avail_out;
        LT_StringBuilder_append_bytes(output, (char*)buffer, produced);

        if (status == Z_STREAM_END){
            break;
        }
        if (status != Z_OK){
            inflateEnd(&stream);
            zlib_error("decompression", status);
        }
        if (produced == 0
                && stream.avail_in == 0
                && input_offset == input_length){
            inflateEnd(&stream);
            LT_error("zlib decompression failed: incomplete input");
        }
    }

    if (stream.avail_in != 0 || input_offset != input_length){
        inflateEnd(&stream);
        LT_error("zlib decompression failed: trailing input");
    }
    status = inflateEnd(&stream);
    if (status != Z_OK){
        zlib_error("decompression finalization", status);
    }
    return (LT_Value)(uintptr_t)LT_ByteVector_new(
        (uint8_t*)LT_StringBuilder_value(output),
        LT_StringBuilder_length(output)
    );
}

LT_DEFINE_PRIMITIVE(
    primitive_zlib_compress,
    "compress",
    "(bytes :optional compression-level)",
    "Compress a bytevector in zlib format. The optional level is -1 through 9."
){
    LT_Value cursor = arguments;
    LT_ByteVector* input;
    LT_Value level_value = LT_SmallInteger_new(Z_DEFAULT_COMPRESSION);
    int level;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, input, LT_ByteVector*, LT_ByteVector_from_value);
    LT_OBJECT_ARG_OPT(cursor, level_value, level_value);
    LT_ARG_END(cursor);
    level = LT_Number_int_from_integer(
        level_value,
        Z_DEFAULT_COMPRESSION,
        Z_BEST_COMPRESSION,
        "zlib compression level must be between -1 and 9"
    );
    return zlib_compress(input, level);
}

LT_DEFINE_PRIMITIVE(
    primitive_zlib_uncompress,
    "uncompress",
    "(bytes)",
    "Decompress a bytevector containing zlib-format data."
){
    LT_Value cursor = arguments;
    LT_ByteVector* input;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, input, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    return zlib_uncompress(input);
}

#define GZIP_STREAM_SELF(cursor, self_value, stream)                         \
    do {                                                                      \
        LT_OBJECT_ARG((cursor), (self_value));                                \
        (stream) = LT_GzipStream_from_value((self_value));                    \
    } while (0)

LT_DEFINE_PRIMITIVE(
    gzip_stream_method_is_closed,
    "GzipStream>>isClosed",
    "(self)",
    "Return true when the gzip stream is closed."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_GzipStream* stream;

    GZIP_STREAM_SELF(cursor, self, stream);
    LT_ARG_END(cursor);
    return stream->closed ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    gzip_stream_method_is_readable,
    "GzipStream>>isReadable",
    "(self)",
    "Return true when the gzip stream supports reading."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_GzipStream* stream;

    GZIP_STREAM_SELF(cursor, self, stream);
    LT_ARG_END(cursor);
    return stream->readable ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    gzip_stream_method_is_writable,
    "GzipStream>>isWritable",
    "(self)",
    "Return true when the gzip stream supports writing."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_GzipStream* stream;

    GZIP_STREAM_SELF(cursor, self, stream);
    LT_ARG_END(cursor);
    return stream->writable ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    gzip_stream_method_close,
    "GzipStream>>close",
    "(self)",
    "Close the gzip stream."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_GzipStream* stream;
    int status;

    GZIP_STREAM_SELF(cursor, self, stream);
    LT_ARG_END(cursor);
    if (stream->closed){
        return self;
    }
    stream->closed = 1;
    status = gzclose(stream->file);
    stream->file = NULL;
    if (status != Z_OK){
        zlib_error("gzip close", status);
    }
    return self;
}

LT_DEFINE_PRIMITIVE(
    gzip_stream_method_flush,
    "GzipStream>>flush",
    "(self)",
    "Flush compressed output."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_GzipStream* stream;
    int status;

    GZIP_STREAM_SELF(cursor, self, stream);
    LT_ARG_END(cursor);
    gzip_stream_check_writable(stream);
    status = gzflush(stream->file, Z_SYNC_FLUSH);
    if (status != Z_OK){
        gzip_stream_error(stream, "flush");
    }
    return self;
}

LT_DEFINE_PRIMITIVE(
    gzip_stream_method_read_byte,
    "GzipStream>>readByte",
    "(self)",
    "Read one byte, or nil at end of file."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_GzipStream* stream;
    int byte;

    GZIP_STREAM_SELF(cursor, self, stream);
    LT_ARG_END(cursor);
    gzip_stream_check_readable(stream);
    byte = gzgetc(stream->file);
    if (byte < 0){
        if (gzeof(stream->file)){
            return LT_NIL;
        }
        gzip_stream_error(stream, "read");
    }
    return LT_SmallInteger_new(byte);
}

LT_DEFINE_PRIMITIVE(
    gzip_stream_method_read_bytes,
    "GzipStream>>readBytes:",
    "(self length)",
    "Read up to length uncompressed bytes."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value length_value;
    LT_GzipStream* stream;
    size_t length;
    size_t count;
    uint8_t* bytes;

    GZIP_STREAM_SELF(cursor, self, stream);
    LT_OBJECT_ARG(cursor, length_value);
    LT_ARG_END(cursor);
    length = LT_Number_nonnegative_size_from_integer(
        length_value,
        "Byte count out of range",
        "Byte count out of range"
    );
    bytes = GC_MALLOC_ATOMIC(length == 0 ? 1 : length);
    count = gzip_stream_read(stream, bytes, length);
    return (LT_Value)(uintptr_t)LT_ByteVector_new(bytes, count);
}

LT_DEFINE_PRIMITIVE(
    gzip_stream_method_read_line,
    "GzipStream>>readLine",
    "(self)",
    "Read a line of uncompressed text."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_GzipStream* stream;
    LT_StringBuilder* builder = LT_StringBuilder_new();
    int byte;

    GZIP_STREAM_SELF(cursor, self, stream);
    LT_ARG_END(cursor);
    gzip_stream_check_readable(stream);
    while ((byte = gzgetc(stream->file)) >= 0){
        if (byte == '\n'){
            char* line = LT_StringBuilder_value(builder);
            size_t length = LT_StringBuilder_length(builder);

            while (length > 0 && line[length - 1] == '\r'){
                length--;
            }
            return (LT_Value)(uintptr_t)LT_String_new(line, length);
        }
        LT_StringBuilder_append_char(builder, (char)byte);
    }
    if (!gzeof(stream->file)){
        gzip_stream_error(stream, "read");
    }
    if (LT_StringBuilder_length(builder) == 0){
        return LT_NIL;
    }
    return (LT_Value)(uintptr_t)LT_String_new(
        LT_StringBuilder_value(builder),
        LT_StringBuilder_length(builder)
    );
}

static LT_ByteVector* gzip_stream_read_to_end(LT_GzipStream* stream){
    LT_StringBuilder* builder = LT_StringBuilder_new();

    while (1){
        uint8_t buffer[8192];
        size_t count = gzip_stream_read(stream, buffer, sizeof(buffer));

        LT_StringBuilder_append_bytes(builder, (char*)buffer, count);
        if (count < sizeof(buffer)){
            break;
        }
    }
    return LT_ByteVector_new(
        (uint8_t*)LT_StringBuilder_value(builder),
        LT_StringBuilder_length(builder)
    );
}

LT_DEFINE_PRIMITIVE(
    gzip_stream_method_read_string,
    "GzipStream>>readString",
    "(self)",
    "Read remaining uncompressed bytes as a string."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_GzipStream* stream;

    GZIP_STREAM_SELF(cursor, self, stream);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_ByteVector_to_string(
        gzip_stream_read_to_end(stream)
    );
}

LT_DEFINE_PRIMITIVE(
    gzip_stream_method_read_bytevector,
    "GzipStream>>readByteVector",
    "(self)",
    "Read remaining uncompressed bytes into a bytevector."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_GzipStream* stream;

    GZIP_STREAM_SELF(cursor, self, stream);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)gzip_stream_read_to_end(stream);
}

LT_DEFINE_PRIMITIVE(
    gzip_stream_method_write_byte,
    "GzipStream>>writeByte:",
    "(self byte)",
    "Write one uncompressed byte."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value byte_value;
    LT_GzipStream* stream;
    uint8_t byte;

    GZIP_STREAM_SELF(cursor, self, stream);
    LT_OBJECT_ARG(cursor, byte_value);
    LT_ARG_END(cursor);
    byte = LT_Number_uint8_from_integer(byte_value, "Byte value out of range");
    gzip_stream_write(stream, &byte, 1);
    return self;
}

LT_DEFINE_PRIMITIVE(
    gzip_stream_method_write_character,
    "GzipStream>>writeCharacter:",
    "(self character)",
    "Write one character as UTF-8."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value character;
    LT_GzipStream* stream;
    char bytes[4];
    size_t length;

    GZIP_STREAM_SELF(cursor, self, stream);
    LT_OBJECT_ARG(cursor, character);
    LT_ARG_END(cursor);
    length = LT_utf8_encode(LT_Character_value(character), bytes);
    gzip_stream_write(stream, (uint8_t*)bytes, length);
    return self;
}

LT_DEFINE_PRIMITIVE(
    gzip_stream_method_write_bytevector,
    "GzipStream>>writeByteVector:",
    "(self bytes)",
    "Write uncompressed bytevector contents."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_GzipStream* stream;
    LT_ByteVector* bytes;

    GZIP_STREAM_SELF(cursor, self, stream);
    LT_GENERIC_ARG(cursor, bytes, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    gzip_stream_write(
        stream,
        LT_ByteVector_bytes(bytes),
        LT_ByteVector_length(bytes)
    );
    return self;
}

LT_DEFINE_PRIMITIVE(
    gzip_stream_method_write_string,
    "GzipStream>>writeString:",
    "(self string)",
    "Write uncompressed string bytes."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_GzipStream* stream;
    LT_String* string;

    GZIP_STREAM_SELF(cursor, self, stream);
    LT_GENERIC_ARG(cursor, string, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);
    gzip_stream_write(
        stream,
        (uint8_t*)LT_String_value_cstr(string),
        LT_String_byte_length(string)
    );
    return self;
}

LT_DEFINE_PRIMITIVE(
    gzip_stream_method_write_ln,
    "GzipStream>>writeLn",
    "(self)",
    "Write an uncompressed line-feed byte."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_GzipStream* stream;
    uint8_t byte = '\n';

    GZIP_STREAM_SELF(cursor, self, stream);
    LT_ARG_END(cursor);
    gzip_stream_write(stream, &byte, 1);
    return self;
}

#define DEFINE_GZIP_OPEN_PRIMITIVE(c_name, primitive_name, mode, description) \
    LT_DEFINE_PRIMITIVE(                                                       \
        c_name,                                                               \
        primitive_name,                                                       \
        "(path)",                                                            \
        description                                                           \
    ){                                                                        \
        LT_Value cursor = arguments;                                          \
        LT_String* path;                                                      \
                                                                              \
        (void)tail_call_unwind_marker;                                        \
        (void)invocation_context_kind;                                        \
        (void)invocation_context_data;                                        \
        LT_GENERIC_ARG(cursor, path, LT_String*, LT_String_from_value);        \
        LT_ARG_END(cursor);                                                   \
        return gzip_stream_new(path, mode);                                   \
    }

DEFINE_GZIP_OPEN_PRIMITIVE(
    primitive_gzip_open_for_input,
    "gzip-open-for-input",
    "rb",
    "Open a gzip-compressed file for input."
)

DEFINE_GZIP_OPEN_PRIMITIVE(
    primitive_gzip_open_for_output,
    "gzip-open-for-output",
    "wb",
    "Open a gzip-compressed file for output."
)

DEFINE_GZIP_OPEN_PRIMITIVE(
    primitive_gzip_open_for_append,
    "gzip-open-for-append",
    "ab",
    "Open a gzip-compressed file for appending."
)

#define DEFINE_GZIP_OPEN_CLASS_METHOD(c_name, primitive_name, mode, description) \
    LT_DEFINE_PRIMITIVE(                                                         \
        c_name,                                                                 \
        primitive_name,                                                         \
        "(self path)",                                                         \
        description                                                             \
    ){                                                                          \
        LT_Value cursor = arguments;                                            \
        LT_Value self;                                                          \
        LT_String* path;                                                        \
                                                                                \
        (void)tail_call_unwind_marker;                                          \
        (void)invocation_context_kind;                                          \
        (void)invocation_context_data;                                          \
        LT_OBJECT_ARG(cursor, self);                                            \
        LT_GENERIC_ARG(cursor, path, LT_String*, LT_String_from_value);          \
        LT_ARG_END(cursor);                                                     \
        (void)self;                                                             \
        return gzip_stream_new(path, mode);                                     \
    }

DEFINE_GZIP_OPEN_CLASS_METHOD(
    gzip_stream_class_method_open_for_input,
    "GzipStream class>>openForInput:",
    "rb",
    "Open a gzip-compressed file for input."
)

DEFINE_GZIP_OPEN_CLASS_METHOD(
    gzip_stream_class_method_open_for_output,
    "GzipStream class>>openForOutput:",
    "wb",
    "Open a gzip-compressed file for output."
)

DEFINE_GZIP_OPEN_CLASS_METHOD(
    gzip_stream_class_method_open_for_append,
    "GzipStream class>>openForAppend:",
    "ab",
    "Open a gzip-compressed file for appending."
)

static LT_Method_Descriptor GzipStream_methods[] = {
    {"isClosed", &gzip_stream_method_is_closed},
    {"isReadable", &gzip_stream_method_is_readable},
    {"isWritable", &gzip_stream_method_is_writable},
    {"close", &gzip_stream_method_close},
    {"flush", &gzip_stream_method_flush},
    {"readByte", &gzip_stream_method_read_byte},
    {"readBytes:", &gzip_stream_method_read_bytes},
    {"readLine", &gzip_stream_method_read_line},
    {"readString", &gzip_stream_method_read_string},
    {"readByteVector", &gzip_stream_method_read_bytevector},
    {"writeByte:", &gzip_stream_method_write_byte},
    {"writeCharacter:", &gzip_stream_method_write_character},
    {"writeByteVector:", &gzip_stream_method_write_bytevector},
    {"writeString:", &gzip_stream_method_write_string},
    {"writeLn", &gzip_stream_method_write_ln},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor GzipStream_class_methods[] = {
    {"openForInput:", &gzip_stream_class_method_open_for_input},
    {"openForOutput:", &gzip_stream_class_method_open_for_output},
    {"openForAppend:", &gzip_stream_class_method_open_for_append},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_GzipStream) {
    .superclass = &LT_Stream_class,
    .metaclass_superclass = &LT_Class_class,
    .package = "ListTalk:zlib",
    .name = "GzipStream",
    .documentation = "Stream backed by a gzip-compressed file.",
    .instance_size = sizeof(LT_GzipStream),
    .class_flags = LT_CLASS_FLAG_FINAL,
    .methods = GzipStream_methods,
    .class_methods = GzipStream_class_methods,
};

void ListTalk_zlib_load(LT_Environment* environment){
    LT_Package* package = LT_Package_new("ListTalk:zlib");

    bind_zlib_constant(
        environment,
        package,
        "version",
        (LT_Value)(uintptr_t)LT_String_new_cstr((char*)zlibVersion())
    );
    bind_zlib_primitive(environment, package, &primitive_zlib_compress);
    bind_zlib_primitive(environment, package, &primitive_zlib_uncompress);
    bind_zlib_primitive(environment, package, &primitive_gzip_open_for_input);
    bind_zlib_primitive(environment, package, &primitive_gzip_open_for_output);
    bind_zlib_primitive(environment, package, &primitive_gzip_open_for_append);
    LT_Environment_bind(
        environment,
        LT_Symbol_new_in(package, "GzipStream"),
        LT_STATIC_CLASS(LT_GzipStream),
        LT_ENV_BINDING_FLAG_CONSTANT
    );
    LT_loader_provide(environment, "zlib");
}
