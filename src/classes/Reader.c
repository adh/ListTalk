/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Reader.h>
#include <ListTalk/classes/Condition.h>
#include <ListTalk/classes/BigInteger.h>
#include <ListTalk/classes/ExactComplexNumber.h>
#include <ListTalk/classes/Fraction.h>
#include <ListTalk/classes/Float.h>
#include <ListTalk/classes/InexactComplexNumber.h>
#include <ListTalk/classes/Character.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/RealNumber.h>
#include <ListTalk/classes/RationalNumber.h>
#include <ListTalk/classes/SmallFraction.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/SourceLocation.h>
#include <ListTalk/classes/ImmutableList.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/classes/Vector.h>
#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/conditions.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/stack_trace.h>

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct LT_FileReaderStream_s {
    LT_ReaderStream base;
    FILE* file;
} LT_FileReaderStream;

typedef struct LT_StringReaderStream_s {
    LT_ReaderStream base;
    const char* source;
    size_t index;
    int has_pushed;
    int pushed;
} LT_StringReaderStream;

struct LT_Reader_s {
    LT_Object base;
    LT_Value source_file;
    LT_Value line;
    LT_Value column;
    LT_Value nesting_depth;
    LT_Value previous_line;
    LT_Value previous_column;
    LT_Value previous_nesting_depth;
    int can_unread;
};

static LT_Value read_object_from_first(
    LT_Reader* reader,
    LT_ReaderStream* stream,
    int first
);
static LT_Value read_bracket_form(LT_Reader* reader, LT_ReaderStream* stream);
static LT_Value read_vector_literal(LT_Reader* reader, LT_ReaderStream* stream);
static LT_Value read_bytevector_literal(LT_Reader* reader, LT_ReaderStream* stream);
static LT_Value read_bytevector_string_literal(
    LT_Reader* reader,
    LT_ReaderStream* stream
);
static LT_Value read_character_literal(LT_Reader* reader, LT_ReaderStream* stream);
static LT_Value read_complex_dispatch_literal(
    LT_Reader* reader,
    LT_ReaderStream* stream
);
static LT_Value read_number_token_with_radix(
    LT_Reader* reader,
    LT_ReaderStream* stream,
    unsigned int radix
);
static char* read_token_string(LT_Reader* reader, int first, LT_ReaderStream* stream);
static void reader_reset_position(LT_Reader* reader);
static int reader_getc(LT_Reader* reader, LT_ReaderStream* stream);
static int reader_ungetc(LT_Reader* reader, LT_ReaderStream* stream, int ch);
static void _Noreturn reader_signal_error(
    LT_Reader* reader,
    LT_Class* klass,
    const char* message
);
static void _Noreturn reader_error(LT_Reader* reader, const char* message);
static void _Noreturn reader_incomplete_input(LT_Reader* reader, const char* message);
static LT_Value reader_source_location(LT_Reader* reader);
static LT_Value reader_immutable_list(
    LT_Reader* reader,
    LT_Value source_location,
    size_t count,
    const LT_Value* values,
    LT_Value tail
);
static LT_Value reader_immutable_list_from_builder(
    LT_Reader* reader,
    LT_Value source_location,
    LT_ListBuilder* builder,
    LT_Value tail
);
static LT_Value LT_Value_from_object(LT_Object* obj);

static int file_stream_getc(void* stream){
    LT_FileReaderStream* file_stream = (LT_FileReaderStream*)stream;
    return fgetc(file_stream->file);
}

static int file_stream_ungetc(int c, void* stream){
    LT_FileReaderStream* file_stream = (LT_FileReaderStream*)stream;

    if (c == EOF){
        return EOF;
    }
    return ungetc(c, file_stream->file);
}

static LT_ReaderStreamVTable file_stream_vtable = {
    .getc = file_stream_getc,
    .ungetc = file_stream_ungetc,
};

static int string_stream_getc(void* stream){
    LT_StringReaderStream* string_stream = (LT_StringReaderStream*)stream;
    int ch;

    if (string_stream->has_pushed){
        string_stream->has_pushed = 0;
        return string_stream->pushed;
    }

    ch = (unsigned char)string_stream->source[string_stream->index];
    if (ch == '\0'){
        return EOF;
    }
    string_stream->index++;
    return ch;
}

static int string_stream_ungetc(int c, void* stream){
    LT_StringReaderStream* string_stream = (LT_StringReaderStream*)stream;

    if (c == EOF || string_stream->has_pushed){
        return EOF;
    }

    string_stream->has_pushed = 1;
    string_stream->pushed = c;
    return c;
}

static LT_ReaderStreamVTable string_stream_vtable = {
    .getc = string_stream_getc,
    .ungetc = string_stream_ungetc,
};

static int is_delimiter(int ch){
    return ch == EOF
        || isspace((unsigned char)ch)
        || ch == '('
        || ch == ')'
        || ch == '['
        || ch == ']'
        || ch == '{'
        || ch == '}'
        || ch == '"'
        || ch == '\''
        || ch == '`'
        || ch == ','
        || ch == ';';
}

static long long reader_small_integer_value(LT_Value value){
    return LT_SmallInteger_value(value);
}

static LT_Value reader_small_integer_new(long long value){
    return LT_SmallInteger_new((int64_t)value);
}

static void reader_reset_position(LT_Reader* reader){
    reader->line = reader_small_integer_new(1);
    reader->column = reader_small_integer_new(0);
    reader->nesting_depth = reader_small_integer_new(0);
    reader->previous_line = reader->line;
    reader->previous_column = reader->column;
    reader->previous_nesting_depth = reader->nesting_depth;
    reader->can_unread = 0;
}

static int reader_getc(LT_Reader* reader, LT_ReaderStream* stream){
    int ch;
    long long line;
    long long column;
    long long nesting_depth;

    reader->previous_line = reader->line;
    reader->previous_column = reader->column;
    reader->previous_nesting_depth = reader->nesting_depth;
    reader->can_unread = 1;

    ch = LT_ReaderStream_getc(stream);
    if (ch == EOF){
        return EOF;
    }

    line = reader_small_integer_value(reader->line);
    column = reader_small_integer_value(reader->column);
    nesting_depth = reader_small_integer_value(reader->nesting_depth);

    if (ch == '\n'){
        line++;
        column = 0;
    } else {
        column++;
    }

    if (ch == '(' || ch == '[' || ch == '{'){
        nesting_depth++;
    } else if ((ch == ')' || ch == ']' || ch == '}') && nesting_depth > 0){
        nesting_depth--;
    }

    reader->line = reader_small_integer_new(line);
    reader->column = reader_small_integer_new(column);
    reader->nesting_depth = reader_small_integer_new(nesting_depth);
    return ch;
}

static int reader_ungetc(LT_Reader* reader, LT_ReaderStream* stream, int ch){
    int result = LT_ReaderStream_ungetc(stream, ch);

    if (result == EOF){
        return EOF;
    }
    if (reader->can_unread){
        reader->line = reader->previous_line;
        reader->column = reader->previous_column;
        reader->nesting_depth = reader->previous_nesting_depth;
        reader->can_unread = 0;
    }
    return result;
}

static void _Noreturn reader_signal_error(
    LT_Reader* reader,
    LT_Class* klass,
    const char* message
){
    LT_Value condition = LT_ReaderError_new(
        klass,
        message,
        LT_NIL,
        reader->line,
        reader->column,
        reader->nesting_depth
    );

    LT_signal(condition);
    fprintf(stderr, "Unrecoverable error: %s\n", message);
    LT_print_backtrace(stderr);
#ifdef __APPLE__
    _exit(1);
#else
    abort();
#endif
}

static void _Noreturn reader_error(LT_Reader* reader, const char* message){
    reader_signal_error(reader, &LT_ReaderError_class, message);
}

static void _Noreturn reader_incomplete_input(LT_Reader* reader, const char* message){
    reader_signal_error(
        reader,
        &LT_IncompleteInputSyntaxError_class,
        message
    );
}

static LT_Value reader_source_location(LT_Reader* reader){
    long long line = reader_small_integer_value(reader->line);
    long long column = reader_small_integer_value(reader->column);

    if (line <= 0){
        line = 1;
    }
    if (column <= 0){
        column = 1;
    }

    return LT_SourceLocation_new((unsigned int)(line - 1), (unsigned int)(column - 1));
}

static LT_Value reader_immutable_list(
    LT_Reader* reader,
    LT_Value source_location,
    size_t count,
    const LT_Value* values,
    LT_Value tail
){
    return LT_ImmutableList_new_with_trailer(
        count,
        values,
        tail,
        source_location,
        reader->source_file,
        LT_NIL
    );
}

static LT_Value reader_immutable_list_from_builder(
    LT_Reader* reader,
    LT_Value source_location,
    LT_ListBuilder* builder,
    LT_Value tail
){
    size_t count = LT_ListBuilder_length(builder);
    LT_Value* values = GC_MALLOC(sizeof(LT_Value) * count);
    LT_Value cursor = LT_ListBuilder_value(builder);
    size_t i = 0;

    while (cursor != LT_NIL){
        values[i++] = LT_car(cursor);
        cursor = LT_cdr(cursor);
    }

    return reader_immutable_list(reader, source_location, count, values, tail);
}

static int dot_starts_dotted_pair(LT_Reader* reader, LT_ReaderStream* stream){
    int next = reader_getc(reader, stream);

    if (!is_delimiter(next)){
        reader_ungetc(reader, stream, next);
        return 0;
    }

    if (next != EOF){
        reader_ungetc(reader, stream, next);
    }
    return 1;
}

static int read_non_space_char(LT_Reader* reader, LT_ReaderStream* stream){
    int ch = reader_getc(reader, stream);

    while (1){
        while (ch != EOF && isspace((unsigned char)ch)){
            ch = reader_getc(reader, stream);
        }

        if (ch != ';'){
            return ch;
        }

        while (ch != EOF && ch != '\n'){
            ch = reader_getc(reader, stream);
        }
    }
}

typedef enum LT_ReaderStringLiteralMode_e {
    LT_READER_STRING_LITERAL_STRING,
    LT_READER_STRING_LITERAL_BYTEVECTOR
} LT_ReaderStringLiteralMode;

static int reader_hex_digit_value(int ch){
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

static void reader_append_utf8_codepoint(LT_Reader* reader,
                                         LT_StringBuilder* builder,
                                         uint32_t codepoint){
    if (!LT_Character_codepoint_is_valid(codepoint)){
        reader_error(reader, "Invalid Unicode code point in string escape");
    }

    if (codepoint <= UINT32_C(0x7f)){
        LT_StringBuilder_append_char(builder, (char)codepoint);
    } else if (codepoint <= UINT32_C(0x7ff)){
        LT_StringBuilder_append_char(
            builder,
            (char)(0xc0 | (codepoint >> 6))
        );
        LT_StringBuilder_append_char(
            builder,
            (char)(0x80 | (codepoint & 0x3f))
        );
    } else if (codepoint <= UINT32_C(0xffff)){
        LT_StringBuilder_append_char(
            builder,
            (char)(0xe0 | (codepoint >> 12))
        );
        LT_StringBuilder_append_char(
            builder,
            (char)(0x80 | ((codepoint >> 6) & 0x3f))
        );
        LT_StringBuilder_append_char(
            builder,
            (char)(0x80 | (codepoint & 0x3f))
        );
    } else {
        LT_StringBuilder_append_char(
            builder,
            (char)(0xf0 | (codepoint >> 18))
        );
        LT_StringBuilder_append_char(
            builder,
            (char)(0x80 | ((codepoint >> 12) & 0x3f))
        );
        LT_StringBuilder_append_char(
            builder,
            (char)(0x80 | ((codepoint >> 6) & 0x3f))
        );
        LT_StringBuilder_append_char(
            builder,
            (char)(0x80 | (codepoint & 0x3f))
        );
    }
}

static void reader_append_string_literal_value(LT_Reader* reader,
                                               LT_StringBuilder* builder,
                                               LT_ReaderStringLiteralMode mode,
                                               uint32_t value,
                                               int raw_byte_escape){
    if (mode == LT_READER_STRING_LITERAL_BYTEVECTOR){
        if (raw_byte_escape){
            if (value > UINT8_MAX){
                reader_error(reader, "Bytevector byte escape out of range");
            }
        } else if (value > UINT32_C(0x7f)){
            reader_error(reader, "Bytevector string literal expects ASCII bytes");
        }
        LT_StringBuilder_append_char(builder, (char)value);
        return;
    }

    reader_append_utf8_codepoint(reader, builder, value);
}

static uint32_t reader_read_fixed_hex_escape(LT_Reader* reader,
                                             LT_ReaderStream* stream,
                                             size_t digit_count){
    uint32_t value = 0;
    size_t i;

    for (i = 0; i < digit_count; i++){
        int ch = reader_getc(reader, stream);
        int digit = reader_hex_digit_value(ch);

        if (digit < 0){
            reader_error(reader, "Hex escape expects hexadecimal digits");
        }
        value = (value << 4) | (uint32_t)digit;
    }
    return value;
}

static uint32_t reader_read_variable_hex_escape(LT_Reader* reader,
                                                LT_ReaderStream* stream){
    uint32_t value = 0;
    int ch = reader_getc(reader, stream);
    int digit = reader_hex_digit_value(ch);

    if (digit < 0){
        reader_error(reader, "Hex escape expects hexadecimal digits");
    }

    while (digit >= 0){
        if (value > (UINT32_MAX >> 4)){
            reader_error(reader, "Hex escape out of range");
        }
        value = (value << 4) | (uint32_t)digit;
        ch = reader_getc(reader, stream);
        digit = reader_hex_digit_value(ch);
    }

    if (ch != EOF){
        reader_ungetc(reader, stream, ch);
    }
    return value;
}

static uint32_t reader_read_octal_escape(LT_Reader* reader,
                                         LT_ReaderStream* stream,
                                         int first){
    uint32_t value = (uint32_t)(first - '0');
    size_t i;

    for (i = 1; i < 3; i++){
        int ch = reader_getc(reader, stream);

        if (ch < '0' || ch > '7'){
            if (ch != EOF){
                reader_ungetc(reader, stream, ch);
            }
            break;
        }
        value = (value << 3) | (uint32_t)(ch - '0');
    }
    return value;
}

static void reader_read_string_escape(LT_Reader* reader,
                                      LT_ReaderStream* stream,
                                      LT_StringBuilder* builder,
                                      LT_ReaderStringLiteralMode mode){
    int escaped = reader_getc(reader, stream);
    uint32_t value;
    int raw_byte_escape = 0;

    switch (escaped){
        case 'a':
            value = '\a';
            break;
        case 'b':
            value = '\b';
            break;
        case 'e':
            value = 27;
            break;
        case 'f':
            value = '\f';
            break;
        case 'n':
            value = '\n';
            break;
        case 'r':
            value = '\r';
            break;
        case 't':
            value = '\t';
            break;
        case 'v':
            value = '\v';
            break;
        case '\\':
        case '\'':
        case '"':
        case '?':
            value = (uint32_t)escaped;
            break;
        case 'x':
            if (mode == LT_READER_STRING_LITERAL_BYTEVECTOR){
                value = reader_read_fixed_hex_escape(reader, stream, 2);
                raw_byte_escape = 1;
            } else {
                value = reader_read_variable_hex_escape(reader, stream);
            }
            break;
        case 'u':
            value = reader_read_fixed_hex_escape(reader, stream, 4);
            break;
        case 'U':
            value = reader_read_fixed_hex_escape(reader, stream, 6);
            break;
        case EOF:
            reader_incomplete_input(
                reader,
                "Unterminated escape sequence in string literal"
            );
            return;
        default:
            if (escaped >= '0' && escaped <= '7'){
                value = reader_read_octal_escape(reader, stream, escaped);
                raw_byte_escape = mode == LT_READER_STRING_LITERAL_BYTEVECTOR;
                break;
            }
            value = (uint32_t)escaped;
            break;
    }

    reader_append_string_literal_value(
        reader,
        builder,
        mode,
        value,
        raw_byte_escape
    );
}

static LT_StringBuilder* read_string_literal_bytes(
    LT_Reader* reader,
    LT_ReaderStream* stream,
    LT_ReaderStringLiteralMode mode
){
    LT_StringBuilder* builder = LT_StringBuilder_new();
    int ch = reader_getc(reader, stream);

    while (ch != EOF && ch != '"'){
        if (ch == '\\'){
            reader_read_string_escape(reader, stream, builder, mode);
        } else {
            if (mode == LT_READER_STRING_LITERAL_STRING){
                LT_StringBuilder_append_char(builder, (char)ch);
            } else {
                reader_append_string_literal_value(
                    reader,
                    builder,
                    mode,
                    (uint32_t)ch,
                    0
                );
            }
        }
        ch = reader_getc(reader, stream);
    }

    if (ch != '"'){
        reader_incomplete_input(reader, "Unterminated string literal");
    }

    return builder;
}

static LT_String* read_string_literal(LT_Reader* reader, LT_ReaderStream* stream){
    LT_StringBuilder* builder = read_string_literal_bytes(
        reader,
        stream,
        LT_READER_STRING_LITERAL_STRING
    );

    return LT_String_new(
        LT_StringBuilder_value(builder),
        LT_StringBuilder_length(builder)
    );
}

static LT_Value read_bytevector_string_literal(
    LT_Reader* reader,
    LT_ReaderStream* stream
){
    LT_StringBuilder* builder = read_string_literal_bytes(
        reader,
        stream,
        LT_READER_STRING_LITERAL_BYTEVECTOR
    );

    return LT_Value_from_object((LT_Object*)LT_ByteVector_new(
        (const uint8_t*)LT_StringBuilder_value(builder),
        LT_StringBuilder_length(builder)
    ));
}

typedef struct LT_ReadTokenResult_s {
    char* token;
    size_t last_unescaped_colon;
    int has_unescaped_colon;
    int has_symbol_quoting;
    int first_char_is_unescaped_colon;
} LT_ReadTokenResult;

static LT_ReadTokenResult read_token(
    LT_Reader* reader,
    int first,
    LT_ReaderStream* stream
){
    LT_StringBuilder* builder = LT_StringBuilder_new();
    LT_ReadTokenResult result = {0};
    int ch = first;
    int in_bar_quote = 0;

    while (1){
        if (in_bar_quote){
            if (ch == EOF){
                reader_incomplete_input(reader, "Unterminated |...| symbol quote");
            }
            if (ch == '\\'){
                int escaped = reader_getc(reader, stream);

                if (escaped == EOF){
                    reader_incomplete_input(
                        reader,
                        "Unterminated escape sequence in symbol token"
                    );
                }
                LT_StringBuilder_append_char(builder, (char)escaped);
                result.has_symbol_quoting = 1;
                ch = reader_getc(reader, stream);
                continue;
            }
            if (ch == '|'){
                result.has_symbol_quoting = 1;
                in_bar_quote = 0;
                ch = reader_getc(reader, stream);
                continue;
            }

            LT_StringBuilder_append_char(builder, (char)ch);
            ch = reader_getc(reader, stream);
            continue;
        }

        if (ch == '\\'){
            int escaped = reader_getc(reader, stream);

            if (escaped == EOF){
                reader_incomplete_input(
                    reader,
                    "Unterminated escape sequence in symbol token"
                );
            }
            LT_StringBuilder_append_char(builder, (char)escaped);
            result.has_symbol_quoting = 1;
            ch = reader_getc(reader, stream);
            continue;
        }

        if (ch == '|'){
            result.has_symbol_quoting = 1;
            in_bar_quote = 1;
            ch = reader_getc(reader, stream);
            continue;
        }

        if (is_delimiter(ch)){
            break;
        }

        if (ch == ':'){
            result.last_unescaped_colon = LT_StringBuilder_length(builder);
            result.has_unescaped_colon = 1;
            if (LT_StringBuilder_length(builder) == 0){
                result.first_char_is_unescaped_colon = 1;
            }
        }

        LT_StringBuilder_append_char(builder, (char)ch);
        ch = reader_getc(reader, stream);
    }

    if (ch != EOF){
        reader_ungetc(reader, stream, ch);
    }

    result.token = LT_StringBuilder_value(builder);
    return result;
}

static int token_looks_float(const char* token){
    const char* cursor = token;

    while (*cursor != '\0'){
        if (*cursor == '.' || *cursor == 'e' || *cursor == 'E'){
            return 1;
        }
        cursor++;
    }

    return 0;
}

static int parse_float_token(const char* token, LT_Value* value){
    char* end = NULL;
    double parsed;

    if (!token_looks_float(token)){
        return 0;
    }

    errno = 0;
    parsed = strtod(token, &end);

    if (end == token || *end != '\0' || !isfinite(parsed)){
        return 0;
    }

    *value = LT_Float_new(parsed);
    return 1;
}

static int parse_real_number_token(const char* token, LT_Value* value){
    if (LT_Number_parse_token_with_radix(token, 10, value)){
        return 1;
    }
    return parse_float_token(token, value);
}

static int parse_complex_rectangular_token(const char* token, LT_Value* value){
    size_t length = strlen(token);
    size_t index;
    size_t split = 0;
    char* real_token;
    char* imag_token;
    LT_Value real_part;
    LT_Value imaginary_part;

    if (length < 2 || token[length - 1] != 'i'){
        return 0;
    }

    for (index = 1; index + 1 < length; index++){
        if ((token[index] == '+' || token[index] == '-')
            && token[index - 1] != 'e'
            && token[index - 1] != 'E'){
            split = index;
        }
    }

    if (split == 0){
        real_token = GC_MALLOC_ATOMIC(2);
        real_token[0] = '0';
        real_token[1] = '\0';

        imag_token = GC_MALLOC_ATOMIC(length + 2);
        memcpy(imag_token, token, length - 1);
        imag_token[length - 1] = '\0';
        if (imag_token[0] == '\0'){
            strcpy(imag_token, "1");
        } else if (strcmp(imag_token, "+") == 0){
            strcpy(imag_token, "1");
        } else if (strcmp(imag_token, "-") == 0){
            strcpy(imag_token, "-1");
        }
    } else {
        real_token = GC_MALLOC_ATOMIC(split + 1);
        memcpy(real_token, token, split);
        real_token[split] = '\0';

        imag_token = GC_MALLOC_ATOMIC(length - split);
        memcpy(imag_token, token + split, length - split - 1);
        imag_token[length - split - 1] = '\0';
        if (strcmp(imag_token, "+") == 0){
            strcpy(imag_token, "1");
        } else if (strcmp(imag_token, "-") == 0){
            strcpy(imag_token, "-1");
        }
    }

    if (!parse_real_number_token(real_token, &real_part)
        || !parse_real_number_token(imag_token, &imaginary_part)){
        return 0;
    }

    if (LT_Value_is_instance_of(real_part, LT_STATIC_CLASS(LT_RationalNumber))
        && LT_Value_is_instance_of(imaginary_part, LT_STATIC_CLASS(LT_RationalNumber))){
        *value = LT_ExactComplexNumber_new(real_part, imaginary_part);
        return 1;
    }

    if (!LT_Value_is_instance_of(real_part, LT_STATIC_CLASS(LT_RealNumber))
        || !LT_Value_is_instance_of(imaginary_part, LT_STATIC_CLASS(LT_RealNumber))){
        return 0;
    }

    *value = LT_InexactComplexNumber_new(
        LT_Number_to_double(real_part),
        LT_Number_to_double(imaginary_part)
    );
    return 1;
}

static int parse_complex_polar_token(const char* token, LT_Value* value){
    const char* at = strchr(token, '@');
    char* radius_token;
    char* angle_token;
    size_t radius_length;
    LT_Value radius;
    LT_Value angle;
    double radius_value;
    double angle_value;

    if (at == NULL || strchr(at + 1, '@') != NULL){
        return 0;
    }

    radius_length = (size_t)(at - token);
    if (radius_length == 0 || at[1] == '\0'){
        return 0;
    }

    radius_token = GC_MALLOC_ATOMIC(radius_length + 1);
    memcpy(radius_token, token, radius_length);
    radius_token[radius_length] = '\0';
    angle_token = GC_MALLOC_ATOMIC(strlen(at + 1) + 1);
    strcpy(angle_token, at + 1);

    if (!parse_real_number_token(radius_token, &radius)
        || !parse_real_number_token(angle_token, &angle)
        || !LT_Value_is_instance_of(radius, LT_STATIC_CLASS(LT_RealNumber))
        || !LT_Value_is_instance_of(angle, LT_STATIC_CLASS(LT_RealNumber))){
        return 0;
    }

    radius_value = LT_Number_to_double(radius);
    angle_value = LT_Number_to_double(angle);
    *value = LT_InexactComplexNumber_new(
        radius_value * cos(angle_value),
        radius_value * sin(angle_value)
    );
    return 1;
}

static LT_Value parse_symbol_token_from_reader_token(LT_ReadTokenResult token_result){
    char* token = token_result.token;

    if (token[0] == '\0'){
        LT_error("Symbol token must not be empty");
    }

    if (token_result.first_char_is_unescaped_colon){
        if (token[1] == '\0'){
            LT_error("Keyword symbol must not be empty");
        }
        return LT_Symbol_new_in(LT_PACKAGE_KEYWORD, token + 1);
    }

    if (!token_result.has_unescaped_colon){
        return LT_Package_intern_symbol(LT_get_current_package(), token);
    }

    if (token[token_result.last_unescaped_colon + 1] == '\0'){
        LT_error("Package-prefixed symbol must have non-empty name");
    }

    {
        size_t package_len = token_result.last_unescaped_colon;
        char* package_name = GC_MALLOC_ATOMIC(package_len + 1);
        LT_Package* package;

        memcpy(package_name, token, package_len);
        package_name[package_len] = '\0';
        package = LT_Package_resolve_used_package(
            LT_get_current_package(),
            package_name
        );
        if (package == NULL){
            package = LT_Package_new(package_name);
        }
        return LT_Symbol_new_in(package, token + token_result.last_unescaped_colon + 1);
    }
}

static LT_Value expand_self_slot_accessor(LT_Reader* reader, LT_Value source_location, char* token){
    LT_Value values[2];

    if (token[0] != '.'
        || token[1] == '\0'
        || isdigit((unsigned char)token[1])){
        return 0;
    }

    values[0] = LT_Symbol_new_in(LT_PACKAGE_LISTTALK, "%self-slot");
    values[1] = LT_Symbol_parse_token(token + 1);
    return reader_immutable_list(reader, source_location, 2, values, LT_NIL);
}

static LT_Value read_atom(LT_Reader* reader, int first, LT_ReaderStream* stream){
    LT_Value source_location = reader_source_location(reader);
    LT_ReadTokenResult token_result = read_token(reader, first, stream);
    LT_Value value;
    LT_Value expanded;
    char* token = token_result.token;

    if (!token_result.has_symbol_quoting && strcmp(token, ".") == 0){
        reader_error(reader, "Unexpected dot");
    }

    if (!token_result.has_symbol_quoting && parse_complex_rectangular_token(token, &value)){
        return value;
    }
    if (!token_result.has_symbol_quoting && parse_complex_polar_token(token, &value)){
        return value;
    }
    if (!token_result.has_symbol_quoting
        && LT_Number_parse_token_with_radix(token, 10, &value)){
        return value;
    }
    if (!token_result.has_symbol_quoting && parse_float_token(token, &value)){
        return value;
    }

    if (!token_result.has_symbol_quoting){
        expanded = expand_self_slot_accessor(reader, source_location, token);
        if (expanded != 0){
            return expanded;
        }
    }

    return parse_symbol_token_from_reader_token(token_result);
}

static void consume_dispatch_suffix_or_short(
    LT_Reader* reader,
    LT_ReaderStream* stream,
    const char* suffix
){
    int ch = reader_getc(reader, stream);

    if (is_delimiter(ch)){
        if (ch != EOF){
            reader_ungetc(reader, stream, ch);
        }
        return;
    }

    while (*suffix != '\0'){
        if (ch != (unsigned char)(*suffix)){
            reader_error(reader, "Invalid dispatch macro spelling");
        }
        suffix++;
        if (*suffix != '\0'){
            ch = reader_getc(reader, stream);
        }
    }

    ch = reader_getc(reader, stream);
    if (!is_delimiter(ch)){
        reader_error(reader, "Invalid dispatch macro spelling");
    }
    if (ch != EOF){
        reader_ungetc(reader, stream, ch);
    }
}

static void ensure_dispatch_argument_unused(LT_Reader* reader, int argument){
    if (argument != -1){
        reader_error(reader, "Dispatch macro does not accept numeric argument");
    }
}

static LT_Value read_number_token_with_radix(
    LT_Reader* reader,
    LT_ReaderStream* stream,
    unsigned int radix
){
    int ch = reader_getc(reader, stream);
    char* token;
    LT_Value value;

    if (ch == EOF || is_delimiter(ch)){
        reader_incomplete_input(reader, "Numeric dispatch macro expects token");
    }

    token = read_token_string(reader, ch, stream);
    if (!LT_Number_parse_token_with_radix(token, radix, &value)){
        reader_error(reader, "Invalid numeric token");
    }
    return value;
}

static LT_Value read_complex_dispatch_literal(
    LT_Reader* reader,
    LT_ReaderStream* stream
){
    int ch = read_non_space_char(reader, stream);
    LT_Value real_part;
    LT_Value imaginary_part;

    if (ch != '('){
        reader_error(reader, "#C expects list syntax");
    }

    ch = read_non_space_char(reader, stream);
    if (ch == EOF || ch == ')'){
        reader_error(reader, "#C expects two parts");
    }
    real_part = read_object_from_first(reader, stream, ch);

    ch = read_non_space_char(reader, stream);
    if (ch == EOF || ch == ')'){
        reader_error(reader, "#C expects imaginary part");
    }
    imaginary_part = read_object_from_first(reader, stream, ch);

    ch = read_non_space_char(reader, stream);
    if (ch != ')'){
        reader_error(reader, "#C expects exactly two parts");
    }

    if (LT_Value_is_instance_of(real_part, LT_STATIC_CLASS(LT_RationalNumber))
        && LT_Value_is_instance_of(imaginary_part, LT_STATIC_CLASS(LT_RationalNumber))){
        return LT_ExactComplexNumber_new(real_part, imaginary_part);
    }
    if (LT_Value_is_instance_of(real_part, LT_STATIC_CLASS(LT_RealNumber))
        && LT_Value_is_instance_of(imaginary_part, LT_STATIC_CLASS(LT_RealNumber))){
        return LT_InexactComplexNumber_new(
            LT_Number_to_double(real_part),
            LT_Number_to_double(imaginary_part)
        );
    }

    reader_error(reader, "#C parts must be real numbers");
    return LT_NIL;
}

static LT_Value read_bytevector_dispatch_literal(
    LT_Reader* reader,
    LT_ReaderStream* stream
){
    int ch = reader_getc(reader, stream);

    if (ch != '8'){
        reader_error(reader, "Unknown dispatch macro character");
    }

    ch = reader_getc(reader, stream);
    if (ch != '('){
        reader_error(reader, "#u8 expects vector syntax");
    }
    return read_bytevector_literal(reader, stream);
}

static LT_Value read_dispatch_macro(
    LT_Reader* reader,
    LT_ReaderStream* stream
){
    int ch = reader_getc(reader, stream);
    int argument = -1;

    if (ch == EOF){
        reader_incomplete_input(reader, "Expected dispatch token after '#'");
    }

    if (isdigit((unsigned char)ch)){
        argument = 0;
        do {
            argument = argument * 10 + (ch - '0');
            ch = reader_getc(reader, stream);
        } while (isdigit((unsigned char)ch));
    }

    switch (ch){
        case '(':
            ensure_dispatch_argument_unused(reader, argument);
            return read_vector_literal(reader, stream);
        case '"':
            ensure_dispatch_argument_unused(reader, argument);
            return read_bytevector_string_literal(reader, stream);
        case '\\':
            ensure_dispatch_argument_unused(reader, argument);
            return read_character_literal(reader, stream);
        case '!':
            ensure_dispatch_argument_unused(reader, argument);
            ch = reader_getc(reader, stream);
            while (ch != EOF && ch != '\n'){
                ch = reader_getc(reader, stream);
            }
            ch = read_non_space_char(reader, stream);
            if (ch == EOF){
                reader_incomplete_input(reader, "Unexpected end of input");
            }
            return read_object_from_first(reader, stream, ch);
        case '<':
            ensure_dispatch_argument_unused(reader, argument);
            reader_error(reader, "Unreadable object in input");
            return LT_NIL;
        case 't':
            ensure_dispatch_argument_unused(reader, argument);
            consume_dispatch_suffix_or_short(reader, stream, "rue");
            return LT_TRUE;
        case 'f':
            ensure_dispatch_argument_unused(reader, argument);
            consume_dispatch_suffix_or_short(reader, stream, "alse");
            return LT_FALSE;
        case 'n':
            ensure_dispatch_argument_unused(reader, argument);
            consume_dispatch_suffix_or_short(reader, stream, "il");
            return LT_NIL;
        case 'b':
        case 'B':
            ensure_dispatch_argument_unused(reader, argument);
            return read_number_token_with_radix(reader, stream, 2);
        case 'o':
        case 'O':
            ensure_dispatch_argument_unused(reader, argument);
            return read_number_token_with_radix(reader, stream, 8);
        case 'x':
        case 'X':
            ensure_dispatch_argument_unused(reader, argument);
            return read_number_token_with_radix(reader, stream, 16);
        case 'c':
        case 'C':
            ensure_dispatch_argument_unused(reader, argument);
            return read_complex_dispatch_literal(reader, stream);
        case 'u':
        case 'U':
            ensure_dispatch_argument_unused(reader, argument);
            return read_bytevector_dispatch_literal(reader, stream);
        case 'r':
        case 'R':
            if (argument == -1){
                reader_error(reader, "Radix dispatch macro expects numeric argument");
            }
            return read_number_token_with_radix(reader, stream, (unsigned int)argument);
        default:
            reader_error(reader, "Unknown dispatch macro character");
            return LT_NIL;
    }
}

static LT_Value read_character_literal(LT_Reader* reader, LT_ReaderStream* stream){
    int ch = reader_getc(reader, stream);
    char* token;
    char* end;
    unsigned long parsed;

    if (ch == EOF || is_delimiter(ch)){
        reader_incomplete_input(
            reader,
            "Character literal expects token after '#\\\\'"
        );
    }

    token = read_token_string(reader, ch, stream);

    if (token[0] != '\0' && *LT_String_utf8_next(token) == '\0'){
        return LT_Character_new(LT_String_utf8_codepoint_at(token));
    }
    if (strcmp(token, "space") == 0){
        return LT_Character_new((uint32_t)' ');
    }
    if (strcmp(token, "tab") == 0){
        return LT_Character_new((uint32_t)'\t');
    }
    if (strcmp(token, "newline") == 0){
        return LT_Character_new((uint32_t)'\n');
    }
    if (strcmp(token, "return") == 0){
        return LT_Character_new((uint32_t)'\r');
    }
    if ((token[0] == 'u' || token[0] == 'U')
        && token[1] == '+'
        && token[2] != '\0'){
        errno = 0;
        parsed = strtoul(token + 2, &end, 16);
        if (errno != 0 || *end != '\0'
            || !LT_Character_codepoint_is_valid((uint32_t)parsed)){
            reader_error(reader, "Invalid Unicode code point in character literal");
        }
        return LT_Character_new((uint32_t)parsed);
    }

    reader_error(reader, "Invalid character literal");
    return LT_NIL;
}

static LT_Value LT_Value_from_object(LT_Object* obj){
    return (LT_Value)(uintptr_t)obj;
}

static LT_Value read_vector_literal(LT_Reader* reader, LT_ReaderStream* stream){
    LT_ListBuilder* builder = LT_ListBuilder_new();
    LT_Value cursor;
    size_t length = 0;
    size_t i = 0;
    LT_Vector* vector;
    int ch = read_non_space_char(reader, stream);

    if (ch == ')'){
        return LT_Value_from_object((LT_Object*)LT_Vector_new(0));
    }

    while (1){
        LT_Value item;

        if (ch == EOF){
            reader_incomplete_input(reader, "Unterminated vector literal");
        }
        if (ch == '.'){
            reader_error(reader, "Unexpected dot in vector literal");
        }

        item = read_object_from_first(reader, stream, ch);
        LT_ListBuilder_append(builder, item);
        length++;

        ch = read_non_space_char(reader, stream);
        if (ch == ')'){
            break;
        }
    }

    vector = LT_Vector_new(length);
    cursor = LT_ListBuilder_value(builder);
    while (cursor != LT_NIL){
        LT_Vector_atPut(vector, i, LT_car(cursor));
        i++;
        cursor = LT_cdr(cursor);
    }

    return LT_Value_from_object((LT_Object*)vector);
}

static uint8_t reader_checked_byte(LT_Reader* reader, LT_Value value){
    int64_t byte_value;

    if (!LT_Value_is_fixnum(value)){
        reader_error(reader, "#u8 expects fixnum byte values");
    }

    byte_value = LT_SmallInteger_value(value);
    if (byte_value < 0 || byte_value > UINT8_MAX){
        reader_error(reader, "#u8 byte value out of range");
    }
    return (uint8_t)byte_value;
}

static LT_Value read_bytevector_literal(LT_Reader* reader, LT_ReaderStream* stream){
    LT_ListBuilder* builder = LT_ListBuilder_new();
    LT_Value cursor;
    size_t length = 0;
    size_t i = 0;
    LT_ByteVector* bytevector;
    int ch = read_non_space_char(reader, stream);

    if (ch == ')'){
        return LT_Value_from_object((LT_Object*)LT_ByteVector_new_filled(0, 0));
    }

    while (1){
        LT_Value item;

        if (ch == EOF){
            reader_incomplete_input(reader, "Unterminated bytevector literal");
        }
        if (ch == '.'){
            reader_error(reader, "Unexpected dot in bytevector literal");
        }

        item = read_object_from_first(reader, stream, ch);
        (void)reader_checked_byte(reader, item);
        LT_ListBuilder_append(builder, item);
        length++;

        ch = read_non_space_char(reader, stream);
        if (ch == ')'){
            break;
        }
    }

    bytevector = LT_ByteVector_new_filled(length, 0);
    cursor = LT_ListBuilder_value(builder);
    while (cursor != LT_NIL){
        LT_ByteVector_atPut(bytevector, i, reader_checked_byte(reader, LT_car(cursor)));
        i++;
        cursor = LT_cdr(cursor);
    }

    return LT_Value_from_object((LT_Object*)bytevector);
}

static LT_Value read_quote_syntax(
    LT_Reader* reader,
    LT_ReaderStream* stream
){
    LT_Value source_location = reader_source_location(reader);
    int first = read_non_space_char(reader, stream);
    LT_Value quoted;
    LT_Value values[2];

    if (first == EOF){
        reader_incomplete_input(reader, "Unexpected end of input after quote");
    }

    quoted = read_object_from_first(reader, stream, first);
    values[0] = LT_Symbol_new_in(LT_PACKAGE_LISTTALK, "quote");
    values[1] = quoted;
    return reader_immutable_list(reader, source_location, 2, values, LT_NIL);
}

static LT_Value read_quasiquote_syntax(
    LT_Reader* reader,
    LT_ReaderStream* stream
){
    LT_Value source_location = reader_source_location(reader);
    int first = read_non_space_char(reader, stream);
    LT_Value quoted;
    LT_Value values[2];

    if (first == EOF){
        reader_incomplete_input(
            reader,
            "Unexpected end of input after quasiquote"
        );
    }

    quoted = read_object_from_first(reader, stream, first);
    values[0] = LT_Symbol_new_in(LT_PACKAGE_LISTTALK, "quasiquote");
    values[1] = quoted;
    return reader_immutable_list(reader, source_location, 2, values, LT_NIL);
}

static LT_Value read_unquote_syntax(
    LT_Reader* reader,
    LT_ReaderStream* stream
){
    LT_Value source_location = reader_source_location(reader);
    int first = reader_getc(reader, stream);
    LT_Value quoted;
    LT_Value operator_symbol;
    LT_Value values[2];

    if (first == '@'){
        operator_symbol = LT_Symbol_new_in(LT_PACKAGE_LISTTALK, "unquote-splicing");
    } else {
        operator_symbol = LT_Symbol_new_in(LT_PACKAGE_LISTTALK, "unquote");
        if (first != EOF){
            reader_ungetc(reader, stream, first);
        }
    }

    first = read_non_space_char(reader, stream);
    if (first == EOF){
        reader_incomplete_input(reader, "Unexpected end of input after unquote");
    }

    quoted = read_object_from_first(reader, stream, first);
    values[0] = operator_symbol;
    values[1] = quoted;
    return reader_immutable_list(reader, source_location, 2, values, LT_NIL);
}

static LT_Value read_list(LT_Reader* reader, LT_ReaderStream* stream){
    LT_ListBuilder* builder = LT_ListBuilder_new();
    LT_Value source_location = reader_source_location(reader);
    int ch = read_non_space_char(reader, stream);

    if (ch == ')'){
        return LT_NIL;
    }

    while (1){
        LT_Value item;

        if (ch == EOF){
            reader_incomplete_input(reader, "Unterminated list");
        }
        if (ch == '.' && dot_starts_dotted_pair(reader, stream)){
            int tail_first;
            LT_Value tail_value;
            int closing;

            if (LT_ListBuilder_length(builder) == 0){
                reader_error(reader, "Unexpected dot in list");
            }

            tail_first = read_non_space_char(reader, stream);
            if (tail_first == EOF){
                reader_incomplete_input(reader, "Unterminated dotted pair");
            }

            tail_value = read_object_from_first(reader, stream, tail_first);
            closing = read_non_space_char(reader, stream);
            if (closing != ')'){
                reader_error(reader, "Expected ')' after dotted pair tail");
            }
            return reader_immutable_list_from_builder(
                reader,
                source_location,
                builder,
                tail_value
            );
        }

        item = read_object_from_first(reader, stream, ch);
        LT_ListBuilder_append(builder, item);

        ch = read_non_space_char(reader, stream);

        if (ch == ')'){
            return reader_immutable_list_from_builder(
                reader,
                source_location,
                builder,
                LT_NIL
            );
        }
    }
}

static char* read_token_string(LT_Reader* reader, int first, LT_ReaderStream* stream){
    return read_token(reader, first, stream).token;
}

static int token_ends_with_colon(char* token){
    size_t length = strlen(token);
    return length > 0 && token[length - 1] == ':';
}

static int token_is_binary_selector(const char* token){
    const char* cursor = token;
    size_t length;

    if (*cursor == '\0'){
        return 0;
    }

    length = strlen(token);
    if (token[length - 1] == ':'){
        return 0;
    }

    while (*cursor != '\0'){
        if (isalnum((unsigned char)*cursor)){
            return 0;
        }
        cursor++;
    }
    return 1;
}

static LT_Value read_bracket_form(LT_Reader* reader, LT_ReaderStream* stream){
    int ch = read_non_space_char(reader, stream);
    LT_Value receiver;
    LT_ListBuilder* arguments;
    LT_StringBuilder* selector;
    LT_Value selector_symbol;
    LT_Value source_location = reader_source_location(reader);
    char* message_token;

    if (ch == EOF){
        reader_incomplete_input(reader, "Unterminated bracket form");
    }
    if (ch == ']'){
        reader_error(reader, "Bracket form expects receiver");
    }

    receiver = read_object_from_first(reader, stream, ch);
    ch = read_non_space_char(reader, stream);
    if (ch == EOF){
        reader_incomplete_input(reader, "Unterminated bracket form");
    }
    if (ch == ']'){
        reader_error(reader, "Bracket form expects message");
    }
    if (is_delimiter(ch)){
        reader_error(reader, "Bracket form expects token message");
    }

    message_token = read_token_string(reader, ch, stream);
    ch = read_non_space_char(reader, stream);

    if (ch == ']'){
        LT_Value values[3];

        values[0] = LT_Symbol_new_in(LT_PACKAGE_LISTTALK, "send");
        values[1] = receiver;
        values[2] = LT_Symbol_new_in(LT_PACKAGE_KEYWORD, message_token);
        return reader_immutable_list(reader, source_location, 3, values, LT_NIL);
    }

    if (token_is_binary_selector(message_token)){
        LT_Value argument;

        if (ch == EOF){
            reader_incomplete_input(reader, "Unterminated bracket form");
        }

        argument = read_object_from_first(reader, stream, ch);
        ch = read_non_space_char(reader, stream);

        if (ch != ']'){
            if (ch == EOF){
                reader_incomplete_input(reader, "Unterminated bracket form");
            }
            reader_error(reader, "Binary bracket send expects exactly one argument");
        }

        LT_Value values[4];

        values[0] = LT_Symbol_new_in(LT_PACKAGE_LISTTALK, "send");
        values[1] = receiver;
        values[2] = LT_Symbol_new_in(LT_PACKAGE_KEYWORD, message_token);
        values[3] = argument;
        return reader_immutable_list(reader, source_location, 4, values, LT_NIL);
    }

    if (!token_ends_with_colon(message_token)){
        reader_error(
            reader,
            "Keyword bracket send expects selector parts ending with ':'"
        );
    }

    selector = LT_StringBuilder_new();
    arguments = LT_ListBuilder_new();
    LT_StringBuilder_append_str(selector, message_token);

    while (1){
        LT_Value argument;
        char* next_selector_token;

        if (ch == EOF){
            reader_incomplete_input(reader, "Unterminated bracket form");
        }

        argument = read_object_from_first(reader, stream, ch);
        LT_ListBuilder_append(arguments, argument);

        ch = read_non_space_char(reader, stream);
        if (ch == EOF){
            reader_incomplete_input(reader, "Unterminated bracket form");
        }
        if (ch == ']'){
            break;
        }
        if (is_delimiter(ch)){
            reader_error(reader, "Keyword bracket send expects token selector parts");
        }

        next_selector_token = read_token_string(reader, ch, stream);
        if (!token_ends_with_colon(next_selector_token)){
            reader_error(
                reader,
                "Keyword bracket send expects selector parts ending with ':'"
            );
        }
        LT_StringBuilder_append_str(selector, next_selector_token);

        ch = read_non_space_char(reader, stream);
    }

    selector_symbol = LT_Symbol_new_in(
        LT_PACKAGE_KEYWORD,
        LT_StringBuilder_value(selector)
    );

    {
        size_t argument_count = LT_ListBuilder_length(arguments);
        LT_Value* values = GC_MALLOC(sizeof(LT_Value) * (argument_count + 3));
        LT_Value cursor = LT_ListBuilder_value(arguments);
        size_t i = 0;

        values[0] = LT_Symbol_new_in(LT_PACKAGE_LISTTALK, "send");
        values[1] = receiver;
        values[2] = selector_symbol;
        while (cursor != LT_NIL){
            values[i + 3] = LT_car(cursor);
            i++;
            cursor = LT_cdr(cursor);
        }
        return reader_immutable_list(
            reader,
            source_location,
            argument_count + 3,
            values,
            LT_NIL
        );
    }
}

static LT_Value read_object_from_first(
    LT_Reader* reader,
    LT_ReaderStream* stream,
    int first
){
    if (first == EOF){
        reader_incomplete_input(reader, "Unexpected end of input");
    }
    if (first == '"'){
        return LT_Value_from_object((LT_Object*)read_string_literal(reader, stream));
    }
    if (first == '('){
        return read_list(reader, stream);
    }
    if (first == '['){
        return read_bracket_form(reader, stream);
    }
    if (first == ')'){
        reader_error(reader, "Unexpected ')'");
    }
    if (first == ']'){
        reader_error(reader, "Unexpected ']'");
    }
    if (first == '\''){
        return read_quote_syntax(reader, stream);
    }
    if (first == '`'){
        return read_quasiquote_syntax(reader, stream);
    }
    if (first == ','){
        return read_unquote_syntax(reader, stream);
    }
    if (first == '#'){
        return read_dispatch_macro(reader, stream);
    }

    return read_atom(reader, first, stream);
}

static LT_Slot_Descriptor Reader_slots[] = {
    {"source-file", offsetof(LT_Reader, source_file), &LT_SlotType_ReadonlyObject},
    {"line", offsetof(LT_Reader, line), &LT_SlotType_ReadonlyObject},
    {"column", offsetof(LT_Reader, column), &LT_SlotType_ReadonlyObject},
    {
        "nesting-depth",
        offsetof(LT_Reader, nesting_depth),
        &LT_SlotType_ReadonlyObject
    },
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Reader) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Reader",
    .instance_size = sizeof(LT_Reader),
    .slots = Reader_slots,
};

LT_ReaderStream* LT_ReaderStream_newForFile(FILE* file){
    LT_FileReaderStream* stream = GC_NEW(LT_FileReaderStream);
    stream->base.vtable = &file_stream_vtable;
    stream->file = file;
    return &stream->base;
}

LT_ReaderStream* LT_ReaderStream_newForString(const char* str){
    LT_StringReaderStream* stream = GC_NEW(LT_StringReaderStream);
    stream->base.vtable = &string_stream_vtable;
    stream->source = str;
    stream->index = 0;
    stream->has_pushed = 0;
    stream->pushed = EOF;
    return &stream->base;
}

size_t LT_ReaderStream_stringOffset(LT_ReaderStream* stream){
    LT_StringReaderStream* string_stream = (LT_StringReaderStream*)stream;
    return string_stream->index;
}

LT_Reader* LT_Reader_new(LT_Value source_file){
    LT_Reader* reader = LT_Class_ALLOC(LT_Reader);
    reader->source_file = source_file;
    reader_reset_position(reader);
    return reader;
}

LT_Reader* LT_Reader_clone(LT_Reader* reader){
    LT_Reader* clone = LT_Class_ALLOC(LT_Reader);

    clone->source_file = reader->source_file;
    clone->line = reader->line;
    clone->column = reader->column;
    clone->nesting_depth = reader->nesting_depth;
    clone->previous_line = reader->previous_line;
    clone->previous_column = reader->previous_column;
    clone->previous_nesting_depth = reader->previous_nesting_depth;
    clone->can_unread = reader->can_unread;
    return clone;
}

LT_Value LT_Reader_readObject(LT_Reader* reader, LT_ReaderStream* stream){
    int first;

    first = read_non_space_char(reader, stream);

    if (first == EOF){
        reader_incomplete_input(reader, "Unexpected end of input");
    }

    return read_object_from_first(reader, stream, first);
}
