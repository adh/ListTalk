#include <ListTalk/Reader.h>
#include <ListTalk/utils.h>

#include <ListTalk/Symbol.h>
#include <ListTalk/Array.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>

#define TOKEN_EOF    0
#define TOKEN_ATOM   'A'
#define TOKEN_STRING 'S'

typedef struct LT_Reader_Token {
    int type;
    char* value;
} LT_Reader_Token;

typedef struct LT_Reader_Tokenizer {
    LT_Reader_Token cur_token;
    int line;
    int column;
    int unread;
    FILE* stream;
} LT_Reader_Tokenizer;

LT_Reader_Tokenizer* tokenizer_new(FILE* stream){
    LT_Reader_Tokenizer* tokenizer = GC_NEW(LT_Reader_Tokenizer);
    tokenizer->stream = stdin;
    tokenizer->line = 1;
    tokenizer->column = 1;
    tokenizer->unread = -1;
    return tokenizer;
}

static int read_char(LT_Reader_Tokenizer* tokenizer){
    if (tokenizer->unread > 0){
        int res = tokenizer->unread;
        tokenizer->unread = -1;
        return res;
    }
    int res = fgetc(tokenizer->stream);
    if (res == '\n'){
        tokenizer->column=1;
        tokenizer->line++;
    } else {
        tokenizer->column++;
    }
    return res;
}
static void unread_char(LT_Reader_Tokenizer* tokenizer, int ch){
    tokenizer->unread = ch;
}


static void read_token(LT_Reader_Tokenizer* tokenizer){
    int ch;
    
    tokenizer->cur_token.value = NULL;

    do {
        ch = read_char(tokenizer);

        if (ch == ';'){
            do {
                ch = read_char(tokenizer);
            } while (ch != '\n');
        }

    } while (strchr(" \t\n\r\f", ch));


    if (ch == EOF){
        tokenizer->cur_token.type = TOKEN_EOF;
        return;
    }
    if (strchr("()[]", ch)){ /* single character tokens */
        tokenizer->cur_token.type = ch;
        return;
    }

    if (ch=='"'){
        int prev_ch = 0;
        tokenizer->cur_token.type = TOKEN_STRING;
        LT_StringBuilder* builder = LT_StringBuilder_new();

        for (;;) {
            ch = read_char(tokenizer);
            if (ch == '"' && prev_ch != '\\'){
                break;
            }
            prev_ch = ch;
            LT_StringBuilder_append_char(builder, ch);
        }
        tokenizer->cur_token.value = LT_StringBuilder_value(builder);
        return;
    }

    tokenizer->cur_token.type = TOKEN_ATOM;
    LT_StringBuilder* builder = LT_StringBuilder_new();
    LT_StringBuilder_append_char(builder, ch);

    for (;;) {
        ch = read_char(tokenizer);
        if (strchr(" \t\n\r\f()[];", ch)){
            break;
        }
        LT_StringBuilder_append_char(builder, ch);
    }
    unread_char(tokenizer, ch);
    tokenizer->cur_token.value = LT_StringBuilder_value(builder);
}

struct LT_Reader {
    LT_Object base;
    LT_Reader_Tokenizer* tokenizer;
};

LT_DEFINE_CLASS(LT_Reader){
    .instance_size = sizeof(LT_Reader),
};

LT_Reader* LT_Reader_newForStream(FILE* stream){
    LT_Reader* reader = LT_Class_ALLOC(LT_Reader);

    reader->tokenizer = tokenizer_new(stream);

    return reader;
}

static LT_Object* dispatch_object(LT_Reader* reader);

static LT_Object* dispatch_tuple(LT_Reader* reader){
    LT_ImmutableTupleBuilder* builder = LT_ImmutableTupleBuilder_new();   
    for(;;){
        read_token(reader->tokenizer);
        if (reader->tokenizer->cur_token.type == ')'){
            LT_Object* obj = LT_ImmutableTupleBuilder_value(builder);
            fprintf(stderr, ";; dispatch_tuple result: ");
            LT_Object_printOn(obj, stderr);
            fprintf(stderr, "\n");
            return obj;
        }
        LT_ImmutableTupleBuilder_add(builder, dispatch_object(reader));
    }

}

static LT_Object* dispatch_object(LT_Reader* reader){
    assert(reader->tokenizer->cur_token.type != TOKEN_EOF);

    switch(reader->tokenizer->cur_token.type){
        case TOKEN_ATOM:
            return LT_Symbol_new(reader->tokenizer->cur_token.value);
        case TOKEN_STRING: /* TODO: no String class for now */
            return LT_Symbol_new(reader->tokenizer->cur_token.value);    
        case '(':
            return dispatch_tuple(reader);
        default:
            assert(0);
    }
}

LT_Object* LT_Reader_readObject(LT_Reader* reader){
    read_token(reader->tokenizer);
    return dispatch_object(reader);
}