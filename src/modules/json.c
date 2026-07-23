/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/Dictionary.h>
#include <ListTalk/classes/Float.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Package.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/loader.h>

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

LT_DECLARE_CLASS(LT_JSONDecoder);
LT_DECLARE_CLASS(LT_JSONEncoder);

struct LT_JSONDecoder_s {
    LT_Object base;
    LT_Value array_conversion;
    LT_Value object_conversion;
};

struct LT_JSONEncoder_s {
    LT_Object base;
    LT_Value output;
    LT_Value pretty;
    LT_Value indent;
};

typedef struct {
    const char* cursor;
    const char* end;
    LT_JSONDecoder* decoder;
} JSONParser;

typedef struct {
    LT_StringBuilder* builder;
    LT_JSONEncoder* encoder;
} JSONEmitter;

static int keyword_p(LT_Value value, const char* name){
    LT_Symbol* symbol;

    if (!LT_Symbol_p(value)){
        return 0;
    }
    symbol = LT_Symbol_from_value(value);
    return LT_Symbol_package(symbol) == LT_PACKAGE_KEYWORD
        && strcmp(LT_Symbol_name(symbol), name) == 0;
}

static LT_Value keyword(const char* name){
    return LT_Symbol_new_in(LT_PACKAGE_KEYWORD, (char*)name);
}

static LT_JSONDecoder* json_decoder_new(LT_Value array_conversion,
                                        LT_Value object_conversion){
    LT_JSONDecoder* decoder = LT_Class_ALLOC(LT_JSONDecoder);

    decoder->array_conversion = array_conversion;
    decoder->object_conversion = object_conversion;
    return decoder;
}

static LT_JSONEncoder* json_encoder_new(void){
    LT_JSONEncoder* encoder = LT_Class_ALLOC(LT_JSONEncoder);

    encoder->output = keyword("string");
    encoder->pretty = LT_FALSE;
    encoder->indent = LT_SmallInteger_new(2);
    return encoder;
}

static void append_utf8_codepoint(LT_StringBuilder* builder, uint32_t codepoint){
    if (codepoint <= 0x7f){
        LT_StringBuilder_append_char(builder, (char)codepoint);
    } else if (codepoint <= 0x7ff){
        LT_StringBuilder_append_char(builder, (char)(0xc0 | (codepoint >> 6)));
        LT_StringBuilder_append_char(builder, (char)(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff){
        LT_StringBuilder_append_char(builder, (char)(0xe0 | (codepoint >> 12)));
        LT_StringBuilder_append_char(builder, (char)(0x80 | ((codepoint >> 6) & 0x3f)));
        LT_StringBuilder_append_char(builder, (char)(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0x10ffff){
        LT_StringBuilder_append_char(builder, (char)(0xf0 | (codepoint >> 18)));
        LT_StringBuilder_append_char(builder, (char)(0x80 | ((codepoint >> 12) & 0x3f)));
        LT_StringBuilder_append_char(builder, (char)(0x80 | ((codepoint >> 6) & 0x3f)));
        LT_StringBuilder_append_char(builder, (char)(0x80 | (codepoint & 0x3f)));
    } else {
        LT_error("Invalid JSON Unicode escape");
    }
}

static void json_skip_ws(JSONParser* parser){
    while (parser->cursor < parser->end
           && isspace((unsigned char)*parser->cursor)){
        parser->cursor++;
    }
}

static void json_expect(JSONParser* parser, char ch){
    if (parser->cursor >= parser->end || *parser->cursor != ch){
        LT_error("Unexpected JSON input");
    }
    parser->cursor++;
}

static int json_hex_digit(int ch){
    if (ch >= '0' && ch <= '9'){
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f'){
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F'){
        return ch - 'A' + 10;
    }
    return -1;
}

static uint32_t json_parse_hex4(JSONParser* parser){
    uint32_t value = 0;
    size_t i;

    for (i = 0; i < 4; i++){
        int digit;

        if (parser->cursor >= parser->end){
            LT_error("Unterminated JSON Unicode escape");
        }
        digit = json_hex_digit((unsigned char)*parser->cursor++);
        if (digit < 0){
            LT_error("Invalid JSON Unicode escape");
        }
        value = (value << 4) | (uint32_t)digit;
    }
    return value;
}

static uint32_t json_parse_escaped_codepoint(JSONParser* parser){
    uint32_t codepoint = json_parse_hex4(parser);

    if (codepoint >= 0xd800 && codepoint <= 0xdbff){
        uint32_t low;

        if (parser->end - parser->cursor < 6
            || parser->cursor[0] != '\\'
            || parser->cursor[1] != 'u'){
            LT_error("Missing JSON low surrogate");
        }
        parser->cursor += 2;
        low = json_parse_hex4(parser);
        if (low < 0xdc00 || low > 0xdfff){
            LT_error("Invalid JSON low surrogate");
        }
        codepoint = 0x10000
            + (((codepoint - 0xd800) << 10) | (low - 0xdc00));
    } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff){
        LT_error("Unexpected JSON low surrogate");
    }
    return codepoint;
}

static LT_Value json_parse_value(JSONParser* parser);

static LT_Value json_parse_string(JSONParser* parser){
    LT_StringBuilder* builder = LT_StringBuilder_new();

    json_expect(parser, '"');
    while (parser->cursor < parser->end){
        unsigned char ch = (unsigned char)*parser->cursor++;

        if (ch == '"'){
            return (LT_Value)(uintptr_t)LT_String_new(
                LT_StringBuilder_value(builder),
                LT_StringBuilder_length(builder)
            );
        }
        if (ch < 0x20){
            LT_error("Control character in JSON string");
        }
        if (ch != '\\'){
            LT_StringBuilder_append_char(builder, (char)ch);
            continue;
        }
        if (parser->cursor >= parser->end){
            LT_error("Unterminated JSON escape");
        }
        ch = (unsigned char)*parser->cursor++;
        switch (ch){
            case '"':
            case '\\':
            case '/':
                LT_StringBuilder_append_char(builder, (char)ch);
                break;
            case 'b':
                LT_StringBuilder_append_char(builder, '\b');
                break;
            case 'f':
                LT_StringBuilder_append_char(builder, '\f');
                break;
            case 'n':
                LT_StringBuilder_append_char(builder, '\n');
                break;
            case 'r':
                LT_StringBuilder_append_char(builder, '\r');
                break;
            case 't':
                LT_StringBuilder_append_char(builder, '\t');
                break;
            case 'u':
                append_utf8_codepoint(builder, json_parse_escaped_codepoint(parser));
                break;
            default:
                LT_error("Invalid JSON escape");
        }
    }
    LT_error("Unterminated JSON string");
    return LT_NIL;
}

static LT_Value json_parse_number(JSONParser* parser){
    const char* start = parser->cursor;
    char* token;
    LT_Value value;
    int inexact = 0;

    if (parser->cursor < parser->end && *parser->cursor == '-'){
        parser->cursor++;
    }
    if (parser->cursor >= parser->end){
        LT_error("Invalid JSON number");
    }
    if (*parser->cursor == '0'){
        parser->cursor++;
    } else if (*parser->cursor >= '1' && *parser->cursor <= '9'){
        while (parser->cursor < parser->end && isdigit((unsigned char)*parser->cursor)){
            parser->cursor++;
        }
    } else {
        LT_error("Invalid JSON number");
    }
    if (parser->cursor < parser->end && *parser->cursor == '.'){
        inexact = 1;
        parser->cursor++;
        if (parser->cursor >= parser->end || !isdigit((unsigned char)*parser->cursor)){
            LT_error("Invalid JSON number");
        }
        while (parser->cursor < parser->end && isdigit((unsigned char)*parser->cursor)){
            parser->cursor++;
        }
    }
    if (parser->cursor < parser->end
        && (*parser->cursor == 'e' || *parser->cursor == 'E')){
        inexact = 1;
        parser->cursor++;
        if (parser->cursor < parser->end
            && (*parser->cursor == '+' || *parser->cursor == '-')){
            parser->cursor++;
        }
        if (parser->cursor >= parser->end || !isdigit((unsigned char)*parser->cursor)){
            LT_error("Invalid JSON number");
        }
        while (parser->cursor < parser->end && isdigit((unsigned char)*parser->cursor)){
            parser->cursor++;
        }
    }

    token = GC_MALLOC_ATOMIC((size_t)(parser->cursor - start) + 1);
    memcpy(token, start, (size_t)(parser->cursor - start));
    token[parser->cursor - start] = '\0';

    if (inexact){
        char* endptr;
        double parsed;

        errno = 0;
        parsed = strtod(token, &endptr);
        if (errno != 0 || *endptr != '\0' || !isfinite(parsed)){
            LT_error("JSON number out of range");
        }
        return LT_Float_new(parsed);
    }
    if (!LT_Number_parse_token_with_radix(token, 10, &value)){
        LT_error("Invalid JSON number");
    }
    return value;
}

static void json_expect_literal(JSONParser* parser,
                                const char* literal,
                                LT_Value result,
                                LT_Value* result_out){
    size_t length = strlen(literal);

    if ((size_t)(parser->end - parser->cursor) < length
        || memcmp(parser->cursor, literal, length) != 0){
        LT_error("Invalid JSON literal");
    }
    parser->cursor += length;
    *result_out = result;
}

static LT_Value json_convert_array(JSONParser* parser, LT_Value list){
    LT_Value conversion = parser->decoder->array_conversion;

    if (keyword_p(conversion, "list")){
        return list;
    }
    if (keyword_p(conversion, "vector")){
        size_t length;
        LT_Value cursor = list;
        LT_Vector* vector;
        size_t i = 0;

        length = 0;
        while (cursor != LT_NIL){
            if (!LT_Pair_p(cursor)){
                LT_error("JSON array conversion expects proper list");
            }
            length++;
            cursor = LT_cdr(cursor);
        }
        vector = LT_Vector_new(length);
        cursor = list;
        while (cursor != LT_NIL){
            LT_Vector_atPut(vector, i++, LT_car(cursor));
            cursor = LT_cdr(cursor);
        }
        return (LT_Value)(uintptr_t)vector;
    }
    return LT_apply(conversion, LT_cons(list, LT_NIL), LT_NIL, LT_NIL, NULL);
}

static LT_Value json_convert_object(JSONParser* parser, LT_Value plist){
    LT_Value conversion = parser->decoder->object_conversion;

    if (keyword_p(conversion, "plist")){
        return plist;
    }
    if (keyword_p(conversion, "dictionary")){
        return (LT_Value)(uintptr_t)LT_Dictionary_newFromAList(
            LT_List_plistToAlist(plist)
        );
    }
    return LT_apply(conversion, LT_cons(plist, LT_NIL), LT_NIL, LT_NIL, NULL);
}

static LT_Value json_parse_array(JSONParser* parser){
    LT_ListBuilder* builder = LT_ListBuilder_new();

    json_expect(parser, '[');
    json_skip_ws(parser);
    if (parser->cursor < parser->end && *parser->cursor == ']'){
        parser->cursor++;
        return json_convert_array(parser, LT_NIL);
    }
    while (1){
        LT_ListBuilder_append(builder, json_parse_value(parser));
        json_skip_ws(parser);
        if (parser->cursor < parser->end && *parser->cursor == ']'){
            parser->cursor++;
            return json_convert_array(parser, LT_ListBuilder_value(builder));
        }
        json_expect(parser, ',');
        json_skip_ws(parser);
    }
}

static LT_Value json_parse_object(JSONParser* parser){
    LT_ListBuilder* builder = LT_ListBuilder_new();

    json_expect(parser, '{');
    json_skip_ws(parser);
    if (parser->cursor < parser->end && *parser->cursor == '}'){
        parser->cursor++;
        return json_convert_object(parser, LT_NIL);
    }
    while (1){
        LT_Value key;
        LT_Value value;

        if (parser->cursor >= parser->end || *parser->cursor != '"'){
            LT_error("JSON object key must be a string");
        }
        key = json_parse_string(parser);
        json_skip_ws(parser);
        json_expect(parser, ':');
        value = json_parse_value(parser);
        LT_ListBuilder_append(builder, key);
        LT_ListBuilder_append(builder, value);
        json_skip_ws(parser);
        if (parser->cursor < parser->end && *parser->cursor == '}'){
            parser->cursor++;
            return json_convert_object(parser, LT_ListBuilder_value(builder));
        }
        json_expect(parser, ',');
        json_skip_ws(parser);
    }
}

static LT_Value json_parse_value(JSONParser* parser){
    LT_Value result;

    json_skip_ws(parser);
    if (parser->cursor >= parser->end){
        LT_error("Unexpected end of JSON input");
    }
    switch (*parser->cursor){
        case 'n':
            json_expect_literal(parser, "null", LT_NIL, &result);
            return result;
        case 't':
            json_expect_literal(parser, "true", LT_TRUE, &result);
            return result;
        case 'f':
            json_expect_literal(parser, "false", LT_FALSE, &result);
            return result;
        case '"':
            return json_parse_string(parser);
        case '[':
            return json_parse_array(parser);
        case '{':
            return json_parse_object(parser);
        default:
            if (*parser->cursor == '-' || isdigit((unsigned char)*parser->cursor)){
                return json_parse_number(parser);
            }
            LT_error("Unexpected JSON input");
    }
    return LT_NIL;
}

static LT_Value json_decode(LT_JSONDecoder* decoder, LT_String* string){
    JSONParser parser;
    LT_Value value;

    parser.cursor = LT_String_value_cstr(string);
    parser.end = parser.cursor + LT_String_byte_length(string);
    parser.decoder = decoder;
    value = json_parse_value(&parser);
    json_skip_ws(&parser);
    if (parser.cursor != parser.end){
        LT_error("Trailing data after JSON value");
    }
    return value;
}

static void json_emit_value(JSONEmitter* emitter, LT_Value value, int depth);

static int json_pretty_p(JSONEmitter* emitter){
    return LT_Value_truthy_p(emitter->encoder->pretty);
}

static int json_indent_width(JSONEmitter* emitter){
    return LT_Number_int_from_integer(
        emitter->encoder->indent,
        0,
        64,
        "JSON indent out of range"
    );
}

static void json_emit_indent(JSONEmitter* emitter, int depth){
    int width = json_indent_width(emitter);
    int i;

    for (i = 0; i < depth * width; i++){
        LT_StringBuilder_append_char(emitter->builder, ' ');
    }
}

static void json_emit_string(JSONEmitter* emitter, LT_String* string){
    const char* cursor = LT_String_value_cstr(string);
    const char* end = cursor + LT_String_byte_length(string);

    LT_StringBuilder_append_char(emitter->builder, '"');
    while (cursor < end){
        unsigned char ch = (unsigned char)*cursor++;

        switch (ch){
            case '"':
                LT_StringBuilder_append_str(emitter->builder, "\\\"");
                break;
            case '\\':
                LT_StringBuilder_append_str(emitter->builder, "\\\\");
                break;
            case '\b':
                LT_StringBuilder_append_str(emitter->builder, "\\b");
                break;
            case '\f':
                LT_StringBuilder_append_str(emitter->builder, "\\f");
                break;
            case '\n':
                LT_StringBuilder_append_str(emitter->builder, "\\n");
                break;
            case '\r':
                LT_StringBuilder_append_str(emitter->builder, "\\r");
                break;
            case '\t':
                LT_StringBuilder_append_str(emitter->builder, "\\t");
                break;
            default:
                if (ch < 0x20){
                    LT_StringBuilder_append_str(
                        emitter->builder,
                        LT_sprintf("\\u%04x", (unsigned int)ch)
                    );
                } else {
                    LT_StringBuilder_append_char(emitter->builder, (char)ch);
                }
        }
    }
    LT_StringBuilder_append_char(emitter->builder, '"');
}

static void json_emit_separator(JSONEmitter* emitter, int depth){
    LT_StringBuilder_append_char(emitter->builder, ',');
    if (json_pretty_p(emitter)){
        LT_StringBuilder_append_char(emitter->builder, '\n');
        json_emit_indent(emitter, depth);
    }
}

static void json_emit_list_array(JSONEmitter* emitter, LT_Value list, int depth){
    int first = 1;

    LT_StringBuilder_append_char(emitter->builder, '[');
    if (list != LT_NIL && json_pretty_p(emitter)){
        LT_StringBuilder_append_char(emitter->builder, '\n');
        json_emit_indent(emitter, depth + 1);
    }
    while (list != LT_NIL){
        if (!LT_Pair_p(list)){
            LT_error("Cannot JSON encode improper list");
        }
        if (!first){
            json_emit_separator(emitter, depth + 1);
        }
        json_emit_value(emitter, LT_car(list), depth + 1);
        first = 0;
        list = LT_cdr(list);
    }
    if (!first && json_pretty_p(emitter)){
        LT_StringBuilder_append_char(emitter->builder, '\n');
        json_emit_indent(emitter, depth);
    }
    LT_StringBuilder_append_char(emitter->builder, ']');
}

static void json_emit_vector(JSONEmitter* emitter, LT_Vector* vector, int depth){
    size_t length = LT_Vector_length(vector);
    size_t i;

    LT_StringBuilder_append_char(emitter->builder, '[');
    if (length > 0 && json_pretty_p(emitter)){
        LT_StringBuilder_append_char(emitter->builder, '\n');
        json_emit_indent(emitter, depth + 1);
    }
    for (i = 0; i < length; i++){
        if (i > 0){
            json_emit_separator(emitter, depth + 1);
        }
        json_emit_value(emitter, LT_Vector_at(vector, i), depth + 1);
    }
    if (length > 0 && json_pretty_p(emitter)){
        LT_StringBuilder_append_char(emitter->builder, '\n');
        json_emit_indent(emitter, depth);
    }
    LT_StringBuilder_append_char(emitter->builder, ']');
}

static void json_emit_key_value(JSONEmitter* emitter,
                                LT_Value key,
                                LT_Value value,
                                int depth){
    if (!LT_String_p(key)){
        LT_error("JSON object keys must be strings");
    }
    json_emit_string(emitter, LT_String_from_value(key));
    LT_StringBuilder_append_char(emitter->builder, ':');
    if (json_pretty_p(emitter)){
        LT_StringBuilder_append_char(emitter->builder, ' ');
    }
    json_emit_value(emitter, value, depth);
}

static void json_emit_plist_object(JSONEmitter* emitter, LT_Value plist, int depth){
    int first = 1;

    LT_StringBuilder_append_char(emitter->builder, '{');
    if (plist != LT_NIL && json_pretty_p(emitter)){
        LT_StringBuilder_append_char(emitter->builder, '\n');
        json_emit_indent(emitter, depth + 1);
    }
    while (plist != LT_NIL){
        LT_Value key;
        LT_Value value;

        if (!LT_Pair_p(plist)){
            LT_error("JSON object plist must be a proper list");
        }
        key = LT_car(plist);
        plist = LT_cdr(plist);
        if (plist == LT_NIL || !LT_Pair_p(plist)){
            LT_error("JSON object plist must contain key/value pairs");
        }
        value = LT_car(plist);
        plist = LT_cdr(plist);
        if (!first){
            json_emit_separator(emitter, depth + 1);
        }
        json_emit_key_value(emitter, key, value, depth + 1);
        first = 0;
    }
    if (!first && json_pretty_p(emitter)){
        LT_StringBuilder_append_char(emitter->builder, '\n');
        json_emit_indent(emitter, depth);
    }
    LT_StringBuilder_append_char(emitter->builder, '}');
}

static void json_emit_dictionary(JSONEmitter* emitter,
                                 LT_Dictionary* dictionary,
                                 int depth){
    json_emit_plist_object(
        emitter,
        LT_List_alistToPlist(LT_Dictionary_asAList(dictionary)),
        depth
    );
}

static void json_emit_number(JSONEmitter* emitter, LT_Value value){
    double double_value;

    if (LT_Float_p(value)){
        double_value = LT_Float_value(value);
        if (!isfinite(double_value)){
            LT_error("Cannot JSON encode non-finite number");
        }
        LT_StringBuilder_append_str(emitter->builder, LT_Number_to_string(value));
        return;
    }

    /* Exact fractions stringify as n/d, which is not valid JSON. */
    if (LT_SmallFraction_p(value) || LT_Fraction_p(value)){
        LT_error("Cannot JSON encode exact fraction; convert to float first");
    }

    LT_StringBuilder_append_str(emitter->builder, LT_Number_to_string(value));
}
static void json_emit_value(JSONEmitter* emitter, LT_Value value, int depth){
    if (value == LT_NIL){
        LT_StringBuilder_append_str(emitter->builder, "null");
    } else if (value == LT_TRUE){
        LT_StringBuilder_append_str(emitter->builder, "true");
    } else if (value == LT_FALSE){
        LT_StringBuilder_append_str(emitter->builder, "false");
    } else if (LT_String_p(value)){
        json_emit_string(emitter, LT_String_from_value(value));
    } else if (LT_Value_is_instance_of(value, LT_STATIC_CLASS(LT_RealNumber))){
        json_emit_number(emitter, value);
    } else if (LT_Vector_p(value)){
        json_emit_vector(emitter, LT_Vector_from_value(value), depth);
    } else if (LT_Dictionary_p(value)){
        json_emit_dictionary(emitter, LT_Dictionary_from_value(value), depth);
    } else if (LT_List_p(value)){
        json_emit_list_array(emitter, value, depth);
    } else {
        LT_error("Unsupported value for JSON encoding");
    }
}

static LT_Value json_encoder_finish(LT_JSONEncoder* encoder,
                                    LT_StringBuilder* builder){
    char* bytes = LT_StringBuilder_value(builder);
    size_t length = LT_StringBuilder_length(builder);

    if (keyword_p(encoder->output, "string")){
        return (LT_Value)(uintptr_t)LT_String_new(bytes, length);
    }
    if (keyword_p(encoder->output, "byte-vector")){
        return (LT_Value)(uintptr_t)LT_ByteVector_new((uint8_t*)bytes, length);
    }
    LT_error("JSONEncoder output must be :string or :byte-vector");
    return LT_NIL;
}

static LT_Value json_encode_value(LT_JSONEncoder* encoder, LT_Value value){
    JSONEmitter emitter;

    emitter.builder = LT_StringBuilder_new();
    emitter.encoder = encoder;
    json_emit_value(&emitter, value, 0);
    return json_encoder_finish(encoder, emitter.builder);
}

static LT_Value json_encode_plist(LT_JSONEncoder* encoder, LT_Value plist){
    JSONEmitter emitter;

    emitter.builder = LT_StringBuilder_new();
    emitter.encoder = encoder;
    json_emit_plist_object(&emitter, plist, 0);
    return json_encoder_finish(encoder, emitter.builder);
}

static void JSONDecoder_debugPrintOn(LT_Value obj, FILE* stream){
    LT_JSONDecoder* decoder = LT_JSONDecoder_from_value(obj);

    fprintf(stream, "#<JSONDecoder %p>", (void*)decoder);
}

static void JSONEncoder_debugPrintOn(LT_Value obj, FILE* stream){
    LT_JSONEncoder* encoder = LT_JSONEncoder_from_value(obj);

    fprintf(stream, "#<JSONEncoder %p>", (void*)encoder);
}

LT_DEFINE_PRIMITIVE(
    json_decoder_class_method_new,
    "JSONDecoder class>>new",
    "(self)",
    "Return a JSON decoder using list arrays and dictionary objects."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    (void)self;
    return (LT_Value)(uintptr_t)json_decoder_new(
        keyword("list"),
        keyword("dictionary")
    );
}

LT_DEFINE_PRIMITIVE(
    json_decoder_class_method_new_with_conversions,
    "JSONDecoder class>>newWithArrayConversion:objectConversion:",
    "(self arrayConversion objectConversion)",
    "Return a JSON decoder with explicit array and object conversion policies."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value array_conversion;
    LT_Value object_conversion;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, array_conversion);
    LT_OBJECT_ARG(cursor, object_conversion);
    LT_ARG_END(cursor);
    (void)self;
    return (LT_Value)(uintptr_t)json_decoder_new(array_conversion, object_conversion);
}

LT_DEFINE_PRIMITIVE(
    json_decoder_method_decode,
    "JSONDecoder>>decode:",
    "(self string)",
    "Decode a JSON string."
){
    LT_Value cursor = arguments;
    LT_JSONDecoder* self;
    LT_String* string;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_JSONDecoder*, LT_JSONDecoder_from_value);
    LT_GENERIC_ARG(cursor, string, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);
    return json_decode(self, string);
}

LT_DEFINE_PRIMITIVE(
    json_decoder_method_decode_bytes,
    "JSONDecoder>>decodeBytes:",
    "(self bytes)",
    "Decode UTF-8 JSON bytes."
){
    LT_Value cursor = arguments;
    LT_JSONDecoder* self;
    LT_ByteVector* bytes;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_JSONDecoder*, LT_JSONDecoder_from_value);
    LT_GENERIC_ARG(cursor, bytes, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    return json_decode(self, LT_ByteVector_to_string(bytes));
}

LT_DEFINE_PRIMITIVE(
    json_decoder_method_array_conversion,
    "JSONDecoder>>arrayConversion",
    "(self)",
    "Return the array conversion policy."
){
    LT_Value cursor = arguments;
    LT_JSONDecoder* self;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_JSONDecoder*, LT_JSONDecoder_from_value);
    LT_ARG_END(cursor);
    return self->array_conversion;
}

LT_DEFINE_PRIMITIVE(
    json_decoder_method_array_conversion_set,
    "JSONDecoder>>arrayConversion:",
    "(self conversion)",
    "Set the array conversion policy and return the receiver."
){
    LT_Value cursor = arguments;
    LT_JSONDecoder* self;
    LT_Value conversion;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_JSONDecoder*, LT_JSONDecoder_from_value);
    LT_OBJECT_ARG(cursor, conversion);
    LT_ARG_END(cursor);
    self->array_conversion = conversion;
    return (LT_Value)(uintptr_t)self;
}

LT_DEFINE_PRIMITIVE(
    json_decoder_method_object_conversion,
    "JSONDecoder>>objectConversion",
    "(self)",
    "Return the object conversion policy."
){
    LT_Value cursor = arguments;
    LT_JSONDecoder* self;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_JSONDecoder*, LT_JSONDecoder_from_value);
    LT_ARG_END(cursor);
    return self->object_conversion;
}

LT_DEFINE_PRIMITIVE(
    json_decoder_method_object_conversion_set,
    "JSONDecoder>>objectConversion:",
    "(self conversion)",
    "Set the object conversion policy and return the receiver."
){
    LT_Value cursor = arguments;
    LT_JSONDecoder* self;
    LT_Value conversion;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_JSONDecoder*, LT_JSONDecoder_from_value);
    LT_OBJECT_ARG(cursor, conversion);
    LT_ARG_END(cursor);
    self->object_conversion = conversion;
    return (LT_Value)(uintptr_t)self;
}

LT_DEFINE_PRIMITIVE(
    json_encoder_class_method_new,
    "JSONEncoder class>>new",
    "(self)",
    "Return a compact JSON encoder producing strings."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    (void)self;
    return (LT_Value)(uintptr_t)json_encoder_new();
}

LT_DEFINE_PRIMITIVE(
    json_encoder_class_method_pretty,
    "JSONEncoder class>>pretty",
    "(self)",
    "Return a pretty-printing JSON encoder producing strings."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_JSONEncoder* encoder;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    (void)self;
    encoder = json_encoder_new();
    encoder->pretty = LT_TRUE;
    return (LT_Value)(uintptr_t)encoder;
}

LT_DEFINE_PRIMITIVE(
    json_encoder_method_encode,
    "JSONEncoder>>encode:",
    "(self value)",
    "Encode a value as JSON."
){
    LT_Value cursor = arguments;
    LT_JSONEncoder* self;
    LT_Value value;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_JSONEncoder*, LT_JSONEncoder_from_value);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return json_encode_value(self, value);
}

LT_DEFINE_PRIMITIVE(
    json_encoder_method_encode_plist,
    "JSONEncoder>>encodePlist:",
    "(self plist)",
    "Encode a property list as a JSON object."
){
    LT_Value cursor = arguments;
    LT_JSONEncoder* self;
    LT_Value plist;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_JSONEncoder*, LT_JSONEncoder_from_value);
    LT_OBJECT_ARG(cursor, plist);
    LT_ARG_END(cursor);
    return json_encode_plist(self, plist);
}

LT_DEFINE_PRIMITIVE(
    json_encoder_method_output,
    "JSONEncoder>>output",
    "(self)",
    "Return the output policy."
){
    LT_Value cursor = arguments;
    LT_JSONEncoder* self;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_JSONEncoder*, LT_JSONEncoder_from_value);
    LT_ARG_END(cursor);
    return self->output;
}

LT_DEFINE_PRIMITIVE(
    json_encoder_method_output_set,
    "JSONEncoder>>output:",
    "(self output)",
    "Set output to :string or :byte-vector and return the receiver."
){
    LT_Value cursor = arguments;
    LT_JSONEncoder* self;
    LT_Value output;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_JSONEncoder*, LT_JSONEncoder_from_value);
    LT_OBJECT_ARG(cursor, output);
    LT_ARG_END(cursor);
    self->output = output;
    return (LT_Value)(uintptr_t)self;
}

LT_DEFINE_PRIMITIVE(
    json_encoder_method_pretty,
    "JSONEncoder>>pretty?",
    "(self)",
    "Return true when pretty formatting is enabled."
){
    LT_Value cursor = arguments;
    LT_JSONEncoder* self;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_JSONEncoder*, LT_JSONEncoder_from_value);
    LT_ARG_END(cursor);
    return self->pretty;
}

LT_DEFINE_PRIMITIVE(
    json_encoder_method_pretty_set,
    "JSONEncoder>>pretty?:",
    "(self pretty)",
    "Set pretty formatting and return the receiver."
){
    LT_Value cursor = arguments;
    LT_JSONEncoder* self;
    LT_Value pretty;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_JSONEncoder*, LT_JSONEncoder_from_value);
    LT_OBJECT_ARG(cursor, pretty);
    LT_ARG_END(cursor);
    self->pretty = LT_Value_truthy_p(pretty) ? LT_TRUE : LT_FALSE;
    return (LT_Value)(uintptr_t)self;
}

LT_DEFINE_PRIMITIVE(
    json_encoder_method_indent,
    "JSONEncoder>>indent",
    "(self)",
    "Return the pretty-print indentation width."
){
    LT_Value cursor = arguments;
    LT_JSONEncoder* self;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_JSONEncoder*, LT_JSONEncoder_from_value);
    LT_ARG_END(cursor);
    return self->indent;
}

LT_DEFINE_PRIMITIVE(
    json_encoder_method_indent_set,
    "JSONEncoder>>indent:",
    "(self width)",
    "Set the pretty-print indentation width and return the receiver."
){
    LT_Value cursor = arguments;
    LT_JSONEncoder* self;
    LT_Value width;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_JSONEncoder*, LT_JSONEncoder_from_value);
    LT_OBJECT_ARG(cursor, width);
    LT_ARG_END(cursor);
    (void)LT_Number_int_from_integer(width, 0, 64, "JSON indent out of range");
    self->indent = width;
    return (LT_Value)(uintptr_t)self;
}

static LT_Slot_Descriptor JSONDecoder_slots[] = {
    {"arrayConversion", offsetof(LT_JSONDecoder, array_conversion), &LT_SlotType_Object},
    {"objectConversion", offsetof(LT_JSONDecoder, object_conversion), &LT_SlotType_Object},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static LT_Method_Descriptor JSONDecoder_methods[] = {
    {"decode:", &json_decoder_method_decode},
    {"decodeBytes:", &json_decoder_method_decode_bytes},
    {"arrayConversion", &json_decoder_method_array_conversion},
    {"arrayConversion:", &json_decoder_method_array_conversion_set},
    {"objectConversion", &json_decoder_method_object_conversion},
    {"objectConversion:", &json_decoder_method_object_conversion_set},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor JSONDecoder_class_methods[] = {
    {"new", &json_decoder_class_method_new},
    {"newWithArrayConversion:objectConversion:", &json_decoder_class_method_new_with_conversions},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_JSONDecoder) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "JSONDecoder",
    .documentation = "Configuration object for decoding JSON values.",
    .instance_size = sizeof(LT_JSONDecoder),
    .class_flags = LT_CLASS_FLAG_FINAL,
    .debugPrintOn = JSONDecoder_debugPrintOn,
    .slots = JSONDecoder_slots,
    .methods = JSONDecoder_methods,
    .class_methods = JSONDecoder_class_methods,
};

static LT_Slot_Descriptor JSONEncoder_slots[] = {
    {"output", offsetof(LT_JSONEncoder, output), &LT_SlotType_Object},
    {"pretty", offsetof(LT_JSONEncoder, pretty), &LT_SlotType_Object},
    {"indent", offsetof(LT_JSONEncoder, indent), &LT_SlotType_Object},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static LT_Method_Descriptor JSONEncoder_methods[] = {
    {"encode:", &json_encoder_method_encode},
    {"encodePlist:", &json_encoder_method_encode_plist},
    {"output", &json_encoder_method_output},
    {"output:", &json_encoder_method_output_set},
    {"pretty?", &json_encoder_method_pretty},
    {"pretty?:", &json_encoder_method_pretty_set},
    {"indent", &json_encoder_method_indent},
    {"indent:", &json_encoder_method_indent_set},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor JSONEncoder_class_methods[] = {
    {"new", &json_encoder_class_method_new},
    {"pretty", &json_encoder_class_method_pretty},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_JSONEncoder) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "JSONEncoder",
    .documentation = "Configuration object for encoding JSON values.",
    .instance_size = sizeof(LT_JSONEncoder),
    .class_flags = LT_CLASS_FLAG_FINAL,
    .debugPrintOn = JSONEncoder_debugPrintOn,
    .slots = JSONEncoder_slots,
    .methods = JSONEncoder_methods,
    .class_methods = JSONEncoder_class_methods,
};

void ListTalk_json_load(LT_Environment* environment){
    LT_Package* package = LT_Package_new("ListTalk-JSON");

    LT_init_native_class(&LT_JSONDecoder_class);
    LT_init_native_class(&LT_JSONEncoder_class);
    LT_Environment_bind(
        environment,
        LT_Symbol_new_in(package, "JSONDecoder"),
        LT_STATIC_CLASS(LT_JSONDecoder),
        LT_ENV_BINDING_FLAG_CONSTANT
    );
    LT_Environment_bind(
        environment,
        LT_Symbol_new_in(package, "JSONEncoder"),
        LT_STATIC_CLASS(LT_JSONEncoder),
        LT_ENV_BINDING_FLAG_CONSTANT
    );
    LT_loader_provide(environment, "json");
}
