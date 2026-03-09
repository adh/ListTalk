/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Package.h>
#include <ListTalk/classes/Reader.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/Vector.h>

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

    if (expect(LT_Symbol_p(value), "symbol tag")){
        return 1;
    }
    if (expect(LT_Value_class(value) == &LT_Symbol_class, "symbol class")){
        return 1;
    }

    symbol = LT_Symbol_from_value(value);
    return expect(strcmp(LT_Symbol_name(symbol), "alpha") == 0, "symbol value");
}

static int test_string(void){
    LT_Value value = read_one("\"a\\n\\\"b\"");
    LT_String* string;

    if (expect(LT_Value_class(value) == &LT_String_class, "string class")){
        return 1;
    }

    string = LT_String_from_value(value);
    return expect(
        strcmp(LT_String_value_cstr(string), "a\n\"b") == 0,
        "string value"
    );
}

static int test_nil_list(void){
    LT_Value value = read_one("()");
    return expect(value == LT_NIL, "empty list is nil");
}

static int test_proper_list(void){
    LT_Value list = read_one("(a b)");
    LT_Value tail;

    if (expect(LT_Pair_p(list), "proper list head pair")){
        return 1;
    }
    if (expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_value(LT_car(list))), "a") == 0,
        "proper list first element"
    )){
        return 1;
    }

    tail = LT_cdr(list);
    if (expect(LT_Pair_p(tail), "proper list second pair")){
        return 1;
    }
    if (expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_value(LT_car(tail))), "b") == 0,
        "proper list second element"
    )){
        return 1;
    }

    return expect(LT_cdr(tail) == LT_NIL, "proper list terminator");
}

static int test_dotted_pair(void){
    LT_Value pair = read_one("(a . b)");

    if (expect(LT_Pair_p(pair), "dotted pair type")){
        return 1;
    }
    if (expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_value(LT_car(pair))), "a") == 0,
        "dotted pair car"
    )){
        return 1;
    }

    return expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_value(LT_cdr(pair))), "b") == 0,
        "dotted pair cdr"
    );
}

static int test_comment_and_whitespace(void){
    LT_Value value = read_one("  ; comment here\n  token");
    return expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_value(value)), "token") == 0,
        "comment and whitespace skipping"
    );
}

static int test_fixnum_literal(void){
    LT_Value value = read_one("12345");

    if (expect(LT_Value_is_fixnum(value), "fixnum tag")){
        return 1;
    }
    if (expect(LT_Value_class(value) == &LT_SmallInteger_class, "fixnum class")){
        return 1;
    }

    return expect(
        LT_SmallInteger_value(value) == 12345,
        "fixnum value"
    );
}

static int test_negative_fixnum_literal(void){
    LT_Value value = read_one("-42");

    if (expect(LT_Value_is_fixnum(value), "negative fixnum tag")){
        return 1;
    }

    return expect(
        LT_SmallInteger_value(value) == -42,
        "negative fixnum value"
    );
}

static int test_symbol_not_number(void){
    LT_Value value = read_one("123abc");

    if (expect(LT_Symbol_p(value), "mixed token is symbol")){
        return 1;
    }

    return expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_value(value)), "123abc") == 0,
        "mixed token symbol value"
    );
}

static int test_dispatch_boolean_true(void){
    LT_Value value = read_one("#t");
    return expect(value == LT_TRUE, "dispatch #t");
}

static int test_dispatch_boolean_false(void){
    LT_Value value = read_one("#false");
    return expect(value == LT_FALSE, "dispatch #false");
}

static int test_dispatch_nil(void){
    LT_Value value = read_one("#nil");
    return expect(value == LT_NIL, "dispatch #nil");
}

static int test_dispatch_nil_short(void){
    LT_Value value = read_one("#n");
    return expect(value == LT_NIL, "dispatch #n");
}

static int test_dispatch_bang_comment(void){
    LT_Value value = read_one("#! read-comment\nalpha");
    return expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_value(value)), "alpha") == 0,
        "dispatch #! line comment"
    );
}

static int test_quote_syntax(void){
    LT_Value value = read_one("'a");
    LT_Value tail;

    if (expect(LT_Pair_p(value), "quote syntax returns list")){
        return 1;
    }
    if (expect(
        LT_Symbol_p(LT_car(value))
            && strcmp(
                LT_Symbol_name(LT_Symbol_from_value(LT_car(value))),
                "quote"
            ) == 0,
        "quote syntax head symbol"
    )){
        return 1;
    }

    tail = LT_cdr(value);
    if (expect(LT_Pair_p(tail), "quote syntax has single argument")){
        return 1;
    }
    if (expect(
        LT_Symbol_p(LT_car(tail))
            && strcmp(
                LT_Symbol_name(LT_Symbol_from_value(LT_car(tail))),
                "a"
            ) == 0,
        "quote syntax quoted value"
    )){
        return 1;
    }

    return expect(LT_cdr(tail) == LT_NIL, "quote syntax argument list end");
}

static int test_symbol_package_interning(void){
    LT_Value default_symbol = LT_Symbol_new("alpha");
    LT_Value listtalk_symbol = LT_Symbol_new_in(LT_PACKAGE_LISTTALK, "alpha");
    LT_Value keyword_symbol = LT_Symbol_new_in(LT_PACKAGE_KEYWORD, "alpha");

    if (expect(default_symbol == listtalk_symbol, "default symbol package")){
        return 1;
    }
    if (expect(default_symbol != keyword_symbol, "symbols differ by package")){
        return 1;
    }

    return expect(
        LT_Symbol_package(LT_Symbol_from_value(keyword_symbol))
            == LT_PACKAGE_KEYWORD,
        "symbol stores package"
    );
}

static int test_package_prefixed_symbol(void){
    LT_Value value = read_one("foo:bar");
    LT_Symbol* symbol = LT_Symbol_from_value(value);

    if (expect(LT_Symbol_p(value), "package-prefixed token is symbol")){
        return 1;
    }
    if (expect(strcmp(LT_Symbol_name(symbol), "bar") == 0, "prefixed symbol name")){
        return 1;
    }
    return expect(
        strcmp(LT_Package_name(LT_Symbol_package(symbol)), "foo") == 0,
        "prefixed symbol package"
    );
}

static int test_package_prefix_last_colon_split(void){
    LT_Value value = read_one("http://example.org:path:item");
    LT_Symbol* symbol = LT_Symbol_from_value(value);

    if (expect(
        strcmp(LT_Symbol_name(symbol), "item") == 0,
        "last-colon split symbol name"
    )){
        return 1;
    }
    return expect(
        strcmp(
            LT_Package_name(LT_Symbol_package(symbol)),
            "http://example.org:path"
        ) == 0,
        "last-colon split package name"
    );
}

static int test_keyword_prefix_symbol(void){
    LT_Value value = read_one(":foo:bar");
    LT_Symbol* symbol = LT_Symbol_from_value(value);

    if (expect(
        strcmp(LT_Package_name(LT_Symbol_package(symbol)), "keyword") == 0,
        "keyword package"
    )){
        return 1;
    }
    return expect(
        strcmp(LT_Symbol_name(symbol), "foo:bar") == 0,
        "keyword symbol keeps internal colons"
    );
}

static int test_bracket_unary_send_syntax(void){
    LT_Value value = read_one("[obj foo]");
    LT_Value tail;
    LT_Value selector;

    if (expect(LT_Pair_p(value), "bracket unary expands to list")){
        return 1;
    }
    if (expect(
        LT_Symbol_p(LT_car(value))
            && strcmp(
                LT_Symbol_name(LT_Symbol_from_value(LT_car(value))),
                "send"
            ) == 0,
        "bracket unary send selector"
    )){
        return 1;
    }

    tail = LT_cdr(value);
    if (expect(LT_Pair_p(tail), "bracket unary has receiver")){
        return 1;
    }
    selector = LT_car(LT_cdr(tail));
    if (expect(LT_Symbol_p(selector), "bracket unary keyword selector")){
        return 1;
    }
    if (expect(
        LT_Symbol_package(LT_Symbol_from_value(selector)) == LT_PACKAGE_KEYWORD,
        "bracket unary selector package"
    )){
        return 1;
    }
    return expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_value(selector)), "foo") == 0,
        "bracket unary selector name"
    );
}

static int test_bracket_keyword_send_syntax(void){
    LT_Value value = read_one("[obj token: bar token: baz]");
    LT_Value tail;
    LT_Value selector;
    LT_Value args;

    if (expect(LT_Pair_p(value), "bracket keyword expands to list")){
        return 1;
    }
    tail = LT_cdr(value);
    if (expect(LT_Pair_p(tail), "bracket keyword has receiver")){
        return 1;
    }
    selector = LT_car(LT_cdr(tail));
    if (expect(LT_Symbol_p(selector), "bracket keyword selector symbol")){
        return 1;
    }
    if (expect(
        LT_Symbol_package(LT_Symbol_from_value(selector)) == LT_PACKAGE_KEYWORD,
        "bracket keyword selector package"
    )){
        return 1;
    }
    if (expect(
        strcmp(
            LT_Symbol_name(LT_Symbol_from_value(selector)),
            "token:token:"
        ) == 0,
        "bracket keyword selector name"
    )){
        return 1;
    }

    args = LT_cdr(LT_cdr(tail));
    if (expect(LT_Pair_p(args), "first keyword argument exists")){
        return 1;
    }
    if (expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_value(LT_car(args))), "bar") == 0,
        "first keyword argument"
    )){
        return 1;
    }
    args = LT_cdr(args);
    if (expect(LT_Pair_p(args), "second keyword argument exists")){
        return 1;
    }
    if (expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_value(LT_car(args))), "baz") == 0,
        "second keyword argument"
    )){
        return 1;
    }
    return expect(LT_cdr(args) == LT_NIL, "keyword argument list end");
}

static int test_slot_accessor_syntax(void){
    LT_Value value = read_one(".slot");
    LT_Value tail;

    if (expect(LT_Pair_p(value), "slot accessor expands to list")){
        return 1;
    }
    if (expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_value(LT_car(value))), "%self-slot") == 0,
        "slot accessor operator"
    )){
        return 1;
    }

    tail = LT_cdr(value);
    if (expect(LT_Pair_p(tail), "slot accessor has symbol argument")){
        return 1;
    }
    if (expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_value(LT_car(tail))), "slot") == 0,
        "slot accessor symbol"
    )){
        return 1;
    }
    return expect(LT_cdr(tail) == LT_NIL, "slot accessor arg list end");
}

static int test_vector_literal_empty(void){
    LT_Value value = read_one("#()");
    LT_Vector* vector;

    if (expect(LT_Value_class(value) == &LT_Vector_class, "empty vector class")){
        return 1;
    }
    vector = LT_Vector_from_value(value);
    return expect(LT_Vector_length(vector) == 0, "empty vector length");
}

static int test_vector_literal_values(void){
    LT_Value value = read_one("#(alpha 42 :beta)");
    LT_Vector* vector;
    LT_Value item;

    if (expect(LT_Value_class(value) == &LT_Vector_class, "vector class")){
        return 1;
    }
    vector = LT_Vector_from_value(value);
    if (expect(LT_Vector_length(vector) == 3, "vector length")){
        return 1;
    }

    item = LT_Vector_at(vector, 0);
    if (expect(
        LT_Symbol_p(item)
            && strcmp(LT_Symbol_name(LT_Symbol_from_value(item)), "alpha") == 0,
        "vector first item"
    )){
        return 1;
    }

    item = LT_Vector_at(vector, 1);
    if (expect(
        LT_Value_is_fixnum(item) && LT_SmallInteger_value(item) == 42,
        "vector second item"
    )){
        return 1;
    }

    item = LT_Vector_at(vector, 2);
    if (expect(LT_Symbol_p(item), "vector third item symbol")){
        return 1;
    }
    if (expect(
        LT_Symbol_package(LT_Symbol_from_value(item)) == LT_PACKAGE_KEYWORD,
        "vector third item keyword package"
    )){
        return 1;
    }
    return expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_value(item)), "beta") == 0,
        "vector third item keyword name"
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
    failures += test_fixnum_literal();
    failures += test_negative_fixnum_literal();
    failures += test_symbol_not_number();
    failures += test_dispatch_boolean_true();
    failures += test_dispatch_boolean_false();
    failures += test_dispatch_nil();
    failures += test_dispatch_nil_short();
    failures += test_dispatch_bang_comment();
    failures += test_quote_syntax();
    failures += test_symbol_package_interning();
    failures += test_package_prefixed_symbol();
    failures += test_package_prefix_last_colon_split();
    failures += test_keyword_prefix_symbol();
    failures += test_bracket_unary_send_syntax();
    failures += test_bracket_keyword_send_syntax();
    failures += test_slot_accessor_syntax();
    failures += test_vector_literal_empty();
    failures += test_vector_literal_values();

    if (failures == 0){
        puts("reader tests passed");
        return 0;
    }

    fprintf(stderr, "%d reader test(s) failed\n", failures);
    return 1;
}
