/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Reader.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/error.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

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
};

static LT_Value read_object_from_first(
    LT_Reader* reader,
    LT_ReaderStream* stream,
    int first
);
static LT_Value read_bracket_form(LT_Reader* reader, LT_ReaderStream* stream);

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

static int read_non_space_char(LT_ReaderStream* stream){
    int ch = LT_ReaderStream_getc(stream);

    while (1){
        while (ch != EOF && isspace((unsigned char)ch)){
            ch = LT_ReaderStream_getc(stream);
        }

        if (ch != ';'){
            return ch;
        }

        while (ch != EOF && ch != '\n'){
            ch = LT_ReaderStream_getc(stream);
        }
    }
}

static LT_String* read_string_literal(LT_ReaderStream* stream){
    LT_StringBuilder* builder = LT_StringBuilder_new();
    int ch = LT_ReaderStream_getc(stream);

    while (ch != EOF && ch != '"'){
        if (ch == '\\'){
            int escaped = LT_ReaderStream_getc(stream);
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
                    LT_error("Unterminated escape sequence in string literal");
                    break;
                default:
                    LT_StringBuilder_append_char(builder, (char)escaped);
                    break;
            }
        } else {
            LT_StringBuilder_append_char(builder, (char)ch);
        }
        ch = LT_ReaderStream_getc(stream);
    }

    if (ch != '"'){
        LT_error("Unterminated string literal");
    }

    return LT_String_new(
        LT_StringBuilder_value(builder),
        LT_StringBuilder_length(builder)
    );
}

static int parse_fixnum_token(const char* token, LT_Value* value){
    char* end = NULL;
    long long parsed;

    errno = 0;
    parsed = strtoll(token, &end, 10);

    if (errno == ERANGE || end == token || *end != '\0'){
        return 0;
    }
    if (!LT_Value_fixnum_in_range((int64_t)parsed)){
        return 0;
    }

    *value = LT_Value_fixnum((int64_t)parsed);
    return 1;
}

static LT_Value parse_symbol_token(char* token){
    return LT_Symbol_parse_token(token);
}

static LT_Value expand_self_slot_accessor(char* token){
    if (token[0] != '.'
        || token[1] == '\0'
        || isdigit((unsigned char)token[1])){
        return 0;
    }

    return LT_cons(
        LT_Symbol_new("%self-slot"),
        LT_cons(
            LT_Symbol_parse_token(token + 1),
            LT_NIL
        )
    );
}

static LT_Value read_atom(int first, LT_ReaderStream* stream){
    LT_StringBuilder* builder = LT_StringBuilder_new();
    LT_Value value;
    LT_Value expanded;
    int ch = first;

    while (!is_delimiter(ch)){
        LT_StringBuilder_append_char(builder, ch);
        ch = LT_ReaderStream_getc(stream);
    }

    if (ch != EOF){
        LT_ReaderStream_ungetc(stream, ch);
    }

    if (parse_fixnum_token(LT_StringBuilder_value(builder), &value)){
        return value;
    }

    expanded = expand_self_slot_accessor(LT_StringBuilder_value(builder));
    if (expanded != 0){
        return expanded;
    }

    return parse_symbol_token(LT_StringBuilder_value(builder));
}

static void consume_dispatch_suffix_or_short(
    LT_ReaderStream* stream,
    const char* suffix
){
    int ch = LT_ReaderStream_getc(stream);

    if (is_delimiter(ch)){
        if (ch != EOF){
            LT_ReaderStream_ungetc(stream, ch);
        }
        return;
    }

    while (*suffix != '\0'){
        if (ch != (unsigned char)(*suffix)){
            LT_error("Invalid dispatch macro spelling");
        }
        suffix++;
        if (*suffix != '\0'){
            ch = LT_ReaderStream_getc(stream);
        }
    }

    ch = LT_ReaderStream_getc(stream);
    if (!is_delimiter(ch)){
        LT_error("Invalid dispatch macro spelling");
    }
    if (ch != EOF){
        LT_ReaderStream_ungetc(stream, ch);
    }
}

static LT_Value read_dispatch_macro(
    LT_Reader* reader,
    LT_ReaderStream* stream
){
    int ch = LT_ReaderStream_getc(stream);

    if (ch == EOF){
        LT_error("Expected dispatch token after '#'");
    }
    if (isdigit((unsigned char)ch)){
        LT_error("Dispatch token after '#' must start with non-numeric character");
    }

    switch (ch){
        case '!':
            ch = LT_ReaderStream_getc(stream);
            while (ch != EOF && ch != '\n'){
                ch = LT_ReaderStream_getc(stream);
            }
            ch = read_non_space_char(stream);
            if (ch == EOF){
                LT_error("Unexpected end of input");
            }
            return read_object_from_first(reader, stream, ch);
        case '<':
            LT_error("Unreadable object in input");
            return LT_NIL;
        case 't':
            consume_dispatch_suffix_or_short(stream, "rue");
            return LT_TRUE;
        case 'f':
            consume_dispatch_suffix_or_short(stream, "alse");
            return LT_FALSE;
        case 'n':
            consume_dispatch_suffix_or_short(stream, "il");
            return LT_NIL;
        default:
            LT_error("Unknown dispatch macro character");
            return LT_NIL;
    }
}

static LT_Value LT_Value_from_object(LT_Object* obj){
    return (LT_Value)(uintptr_t)obj;
}

static LT_Value read_quote_syntax(
    LT_Reader* reader,
    LT_ReaderStream* stream
){
    int first = read_non_space_char(stream);
    LT_Value quoted;

    if (first == EOF){
        LT_error("Unexpected end of input after quote");
    }

    quoted = read_object_from_first(reader, stream, first);
    return LT_cons(
        LT_Symbol_new("quote"),
        LT_cons(quoted, LT_NIL)
    );
}

static LT_Value read_list(LT_Reader* reader, LT_ReaderStream* stream){
    LT_Value head = LT_VALUE_NIL;
    LT_Value tail = LT_VALUE_NIL;
    int ch = read_non_space_char(stream);

    if (ch == ')'){
        return LT_VALUE_NIL;
    }

    while (1){
        LT_Value item;
        LT_Value node;

        if (ch == EOF){
            LT_error("Unterminated list");
        }
        if (ch == '.'){
            LT_error("Unexpected dot in list");
        }

        item = read_object_from_first(reader, stream, ch);
        node = LT_cons(item, LT_VALUE_NIL);

        if (head == LT_VALUE_NIL){
            head = node;
        } else {
            LT_Pair_set_cdr(tail, node);
        }
        tail = node;

        ch = read_non_space_char(stream);

        if (ch == ')'){
            return head;
        }
        if (ch == '.'){
            int tail_first = read_non_space_char(stream);
            LT_Value tail_value;
            int closing;

            if (tail_first == EOF){
                LT_error("Unterminated dotted pair");
            }

            tail_value = read_object_from_first(reader, stream, tail_first);
            closing = read_non_space_char(stream);
            if (closing != ')'){
                LT_error("Expected ')' after dotted pair tail");
            }
            LT_Pair_set_cdr(tail, tail_value);
            return head;
        }
    }
}

static char* read_token_string(int first, LT_ReaderStream* stream){
    LT_StringBuilder* builder = LT_StringBuilder_new();
    int ch = first;

    while (!is_delimiter(ch)){
        LT_StringBuilder_append_char(builder, (char)ch);
        ch = LT_ReaderStream_getc(stream);
    }

    if (ch != EOF){
        LT_ReaderStream_ungetc(stream, ch);
    }

    return LT_StringBuilder_value(builder);
}

static int token_ends_with_colon(char* token){
    size_t length = strlen(token);
    return length > 0 && token[length - 1] == ':';
}

static LT_Value read_bracket_form(LT_Reader* reader, LT_ReaderStream* stream){
    int ch = read_non_space_char(stream);
    LT_Value receiver;
    LT_ListBuilder* arguments;
    LT_StringBuilder* selector;
    LT_Value selector_symbol;
    char* message_token;

    if (ch == EOF){
        LT_error("Unterminated bracket form");
    }
    if (ch == ']'){
        LT_error("Bracket form expects receiver");
    }

    receiver = read_object_from_first(reader, stream, ch);
    ch = read_non_space_char(stream);
    if (ch == EOF){
        LT_error("Unterminated bracket form");
    }
    if (ch == ']'){
        LT_error("Bracket form expects message");
    }
    if (is_delimiter(ch)){
        LT_error("Bracket form expects token message");
    }

    message_token = read_token_string(ch, stream);
    ch = read_non_space_char(stream);

    if (ch == ']'){
        return LT_cons(
            LT_Symbol_new("send"),
            LT_cons(
                receiver,
                LT_cons(
                    LT_Symbol_new_in(LT_PACKAGE_KEYWORD, message_token),
                    LT_NIL
                )
            )
        );
    }

    if (!token_ends_with_colon(message_token)){
        LT_error("Keyword bracket send expects selector parts ending with ':'");
    }

    selector = LT_StringBuilder_new();
    arguments = LT_ListBuilder_new();
    LT_StringBuilder_append_str(selector, message_token);

    while (1){
        LT_Value argument;
        char* next_selector_token;

        if (ch == EOF){
            LT_error("Unterminated bracket form");
        }

        argument = read_object_from_first(reader, stream, ch);
        LT_ListBuilder_append(arguments, argument);

        ch = read_non_space_char(stream);
        if (ch == EOF){
            LT_error("Unterminated bracket form");
        }
        if (ch == ']'){
            break;
        }
        if (is_delimiter(ch)){
            LT_error("Keyword bracket send expects token selector parts");
        }

        next_selector_token = read_token_string(ch, stream);
        if (!token_ends_with_colon(next_selector_token)){
            LT_error("Keyword bracket send expects selector parts ending with ':'");
        }
        LT_StringBuilder_append_str(selector, next_selector_token);

        ch = read_non_space_char(stream);
    }

    selector_symbol = LT_Symbol_new_in(
        LT_PACKAGE_KEYWORD,
        LT_StringBuilder_value(selector)
    );

    return LT_cons(
        LT_Symbol_new("send"),
        LT_cons(
            receiver,
            LT_cons(selector_symbol, LT_ListBuilder_value(arguments))
        )
    );
}

static LT_Value read_object_from_first(
    LT_Reader* reader,
    LT_ReaderStream* stream,
    int first
){
    if (first == EOF){
        LT_error("Unexpected end of input");
    }
    if (first == '"'){
        return LT_Value_from_object((LT_Object*)read_string_literal(stream));
    }
    if (first == '('){
        return read_list(reader, stream);
    }
    if (first == '['){
        return read_bracket_form(reader, stream);
    }
    if (first == ')'){
        LT_error("Unexpected ')'");
    }
    if (first == ']'){
        LT_error("Unexpected ']'");
    }
    if (first == '\''){
        return read_quote_syntax(reader, stream);
    }
    if (first == '#'){
        return read_dispatch_macro(reader, stream);
    }

    return read_atom(first, stream);
}

LT_DEFINE_CLASS(LT_Reader) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Reader",
    .instance_size = sizeof(LT_Reader),
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

LT_Reader* LT_Reader_new(void){
    return LT_Class_ALLOC(LT_Reader);
}

LT_Reader* LT_Reader_clone(LT_Reader* reader){
    (void)reader;
    return LT_Reader_new();
}

LT_Value LT_Reader_readObject(LT_Reader* reader, LT_ReaderStream* stream){
    int first;

    (void)reader;
    first = read_non_space_char(stream);

    if (first == EOF){
        LT_error("Unexpected end of input");
    }

    return read_object_from_first(reader, stream, first);
}
