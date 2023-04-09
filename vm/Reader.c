#include <ListTalk/Reader.h>
#include <ListTalk/utils.h>

#include <stdio.h>
#include <string.h>

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

LT_Reader_Tokenizer* tokenizer_new(){
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

LT_Object* LT_reader_read_object(){
    LT_Reader_Tokenizer* tokenizer = tokenizer_new();
    do {
        read_token(tokenizer);
        printf(";; token: %c %s\n", tokenizer->cur_token.type, tokenizer->cur_token.value);
    } while (tokenizer->cur_token.type != TOKEN_EOF);
    abort();
}