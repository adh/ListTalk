#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Reader.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>

#include <stdio.h>
#include <string.h>

static int fail(const char* message){
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

static int expect(int condition, const char* message){
    if (!condition){
        return fail(message);
    }
    return 0;
}

static LT_Value read_one(const char* source){
    LT_Reader* reader = LT_Reader_new();
    LT_ReaderStream* stream = LT_ReaderStream_newForString(source);
    return LT_Reader_readObject(reader, stream);
}

static int test_symbol(void){
    LT_Value value = read_one("alpha");
    LT_Symbol* symbol;

    if (expect(LT_Value_is_symbol(value), "symbol tag")){
        return 1;
    }
    if (expect(LT_Value_class(value) == &LT_Symbol_class, "symbol class")){
        return 1;
    }

    symbol = LT_Symbol_from_object(value);
    return expect(strcmp(LT_Symbol_name(symbol), "alpha") == 0, "symbol value");
}

static int test_string(void){
    LT_Value value = read_one("\"a\\n\\\"b\"");
    LT_String* string;

    if (expect(LT_Value_class(value) == &LT_String_class, "string class")){
        return 1;
    }

    string = LT_String_from_object(value);
    return expect(
        strcmp(LT_String_value_cstr(string), "a\n\"b") == 0,
        "string value"
    );
}

static int test_nil_list(void){
    LT_Value value = read_one("()");
    return expect(value == LT_VALUE_NIL, "empty list is nil");
}

static int test_proper_list(void){
    LT_Value list = read_one("(a b)");
    LT_Value tail;

    if (expect(LT_Value_is_pair(list), "proper list head pair")){
        return 1;
    }
    if (expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_object(LT_car(list))), "a") == 0,
        "proper list first element"
    )){
        return 1;
    }

    tail = LT_cdr(list);
    if (expect(LT_Value_is_pair(tail), "proper list second pair")){
        return 1;
    }
    if (expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_object(LT_car(tail))), "b") == 0,
        "proper list second element"
    )){
        return 1;
    }

    return expect(LT_cdr(tail) == LT_VALUE_NIL, "proper list terminator");
}

static int test_dotted_pair(void){
    LT_Value pair = read_one("(a . b)");

    if (expect(LT_Value_is_pair(pair), "dotted pair type")){
        return 1;
    }
    if (expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_object(LT_car(pair))), "a") == 0,
        "dotted pair car"
    )){
        return 1;
    }

    return expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_object(LT_cdr(pair))), "b") == 0,
        "dotted pair cdr"
    );
}

static int test_comment_and_whitespace(void){
    LT_Value value = read_one("  ; comment here\n  token");
    return expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_object(value)), "token") == 0,
        "comment and whitespace skipping"
    );
}

int main(void){
    int failures = 0;

    LT_init();

    failures += test_symbol();
    failures += test_string();
    failures += test_nil_list();
    failures += test_proper_list();
    failures += test_dotted_pair();
    failures += test_comment_and_whitespace();

    if (failures == 0){
        puts("reader tests passed");
        return 0;
    }

    fprintf(stderr, "%d reader test(s) failed\n", failures);
    return 1;
}
