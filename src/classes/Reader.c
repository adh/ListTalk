/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Reader.h>
#include <ListTalk/classes/Condition.h>
#include <ListTalk/classes/BigInteger.h>
#include <ListTalk/classes/Fraction.h>
#include <ListTalk/classes/Float.h>
#include <ListTalk/classes/Character.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/SmallFraction.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/classes/Vector.h>
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
static LT_Value read_character_literal(LT_Reader* reader, LT_ReaderStream* stream);
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

static LT_String* read_string_literal(LT_Reader* reader, LT_ReaderStream* stream){
    LT_StringBuilder* builder = LT_StringBuilder_new();
    int ch = reader_getc(reader, stream);

    while (ch != EOF && ch != '"'){
        if (ch == '\\'){
            int escaped = reader_getc(reader, stream);
            switch (escaped){
                case 'n':
                    LT_StringBuilder_append_char(builder, '\n');
                    break;
                case 'r':
                    LT_StringBuilder_append_char(builder, '\r');
                    break;
                case 't':
                    LT_StringBuilder_append_char(builder, '\t');
                    break;
                case '\\':
                case '"':
                    LT_StringBuilder_append_char(builder, (char)escaped);
                    break;
                case EOF:
                    reader_incomplete_input(
                        reader,
                        "Unterminated escape sequence in string literal"
                    );
                    break;
                default:
                    LT_StringBuilder_append_char(builder, (char)escaped);
                    break;
            }
        } else {
            LT_StringBuilder_append_char(builder, (char)ch);
        }
        ch = reader_getc(reader, stream);
    }

    if (ch != '"'){
        reader_incomplete_input(reader, "Unterminated string literal");
    }

    return LT_String_new(
        LT_StringBuilder_value(builder),
        LT_StringBuilder_length(builder)
    );
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

static LT_Value expand_self_slot_accessor(char* token){
    if (token[0] != '.'
        || token[1] == '\0'
        || isdigit((unsigned char)token[1])){
        return 0;
    }

    return LT_list(
        LT_Symbol_new_in(LT_PACKAGE_LISTTALK, "%self-slot"),
        LT_Symbol_parse_token(token + 1),
        LT_INVALID
    );
}

static LT_Value read_atom(LT_Reader* reader, int first, LT_ReaderStream* stream){
    LT_ReadTokenResult token_result = read_token(reader, first, stream);
    LT_Value value;
    LT_Value expanded;
    char* token = token_result.token;

    if (!token_result.has_symbol_quoting && strcmp(token, ".") == 0){
        reader_error(reader, "Unexpected dot");
    }

    if (!token_result.has_symbol_quoting && LT_Number_parse_fraction_token(token, &value)){
        return value;
    }
    if (!token_result.has_symbol_quoting && LT_Number_parse_integer_token(token, &value)){
        return value;
    }
    if (!token_result.has_symbol_quoting && parse_float_token(token, &value)){
        return value;
    }

    if (!token_result.has_symbol_quoting){
        expanded = expand_self_slot_accessor(token);
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

static LT_Value read_dispatch_macro(
    LT_Reader* reader,
    LT_ReaderStream* stream
){
    int ch = reader_getc(reader, stream);

    if (ch == EOF){
        reader_incomplete_input(reader, "Expected dispatch token after '#'");
    }
    if (isdigit((unsigned char)ch)){
        reader_error(
            reader,
            "Dispatch token after '#' must start with non-numeric character"
        );
    }

    switch (ch){
        case '(':
            return read_vector_literal(reader, stream);
        case '\\':
            return read_character_literal(reader, stream);
        case '!':
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
            reader_error(reader, "Unreadable object in input");
            return LT_NIL;
        case 't':
            consume_dispatch_suffix_or_short(reader, stream, "rue");
            return LT_TRUE;
        case 'f':
            consume_dispatch_suffix_or_short(reader, stream, "alse");
            return LT_FALSE;
        case 'n':
            consume_dispatch_suffix_or_short(reader, stream, "il");
            return LT_NIL;
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

    if (token[0] != '\0' && token[1] == '\0'){
        return LT_Character_new((uint32_t)(unsigned char)token[0]);
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

static LT_Value read_quote_syntax(
    LT_Reader* reader,
    LT_ReaderStream* stream
){
    int first = read_non_space_char(reader, stream);
    LT_Value quoted;

    if (first == EOF){
        reader_incomplete_input(reader, "Unexpected end of input after quote");
    }

    quoted = read_object_from_first(reader, stream, first);
    return LT_list(
        LT_Symbol_new_in(LT_PACKAGE_LISTTALK, "quote"),
        quoted,
        LT_INVALID
    );
}

static LT_Value read_quasiquote_syntax(
    LT_Reader* reader,
    LT_ReaderStream* stream
){
    int first = read_non_space_char(reader, stream);
    LT_Value quoted;

    if (first == EOF){
        reader_incomplete_input(
            reader,
            "Unexpected end of input after quasiquote"
        );
    }

    quoted = read_object_from_first(reader, stream, first);
    return LT_list(
        LT_Symbol_new_in(LT_PACKAGE_LISTTALK, "quasiquote"),
        quoted,
        LT_INVALID
    );
}

static LT_Value read_unquote_syntax(
    LT_Reader* reader,
    LT_ReaderStream* stream
){
    int first = reader_getc(reader, stream);
    LT_Value quoted;
    LT_Value operator_symbol;

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
    return LT_list(
        operator_symbol,
        quoted,
        LT_INVALID
    );
}

static LT_Value read_list(LT_Reader* reader, LT_ReaderStream* stream){
    LT_Value head = LT_NIL;
    LT_Value tail = LT_NIL;
    int ch = read_non_space_char(reader, stream);

    if (ch == ')'){
        return LT_NIL;
    }

    while (1){
        LT_Value item;
        LT_Value node;

        if (ch == EOF){
            reader_incomplete_input(reader, "Unterminated list");
        }
        if (ch == '.' && dot_starts_dotted_pair(reader, stream)){
            int tail_first;
            LT_Value tail_value;
            int closing;

            if (head == LT_NIL){
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
            LT_Pair_set_cdr(tail, tail_value);
            return head;
        }

        item = read_object_from_first(reader, stream, ch);
        node = LT_cons(item, LT_NIL);

        if (head == LT_NIL){
            head = node;
        } else {
            LT_Pair_set_cdr(tail, node);
        }
        tail = node;

        ch = read_non_space_char(reader, stream);

        if (ch == ')'){
            return head;
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

static LT_Value read_bracket_form(LT_Reader* reader, LT_ReaderStream* stream){
    int ch = read_non_space_char(reader, stream);
    LT_Value receiver;
    LT_ListBuilder* arguments;
    LT_StringBuilder* selector;
    LT_Value selector_symbol;
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
        return LT_list(
            LT_Symbol_new_in(LT_PACKAGE_LISTTALK, "send"),
            receiver,
            LT_Symbol_new_in(LT_PACKAGE_KEYWORD, message_token),
            LT_INVALID
        );
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

    return LT_list_with_rest(
        LT_Symbol_new_in(LT_PACKAGE_LISTTALK, "send"),
        receiver,
        selector_symbol,
        LT_INVALID,
        LT_ListBuilder_value(arguments)
    );
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

LT_Reader* LT_Reader_new(void){
    LT_Reader* reader = LT_Class_ALLOC(LT_Reader);
    reader_reset_position(reader);
    return reader;
}

LT_Reader* LT_Reader_clone(LT_Reader* reader){
    LT_Reader* clone = LT_Class_ALLOC(LT_Reader);

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
