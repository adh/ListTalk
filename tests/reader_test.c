/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Condition.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Package.h>
#include <ListTalk/classes/Reader.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/Float.h>
#include <ListTalk/classes/Vector.h>
#include <ListTalk/classes/Primitive.h>

#include <stdio.h>
#include <stdint.h>
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

static int expect_symbol_print(LT_Value symbol, const char* expected, const char* message){
    FILE* file = tmpfile();
    char buffer[256];
    size_t length;

    if (file == NULL){
        return fail("tmpfile failed");
    }

    LT_Value_debugPrintOn(symbol, file);
    fflush(file);
    fseek(file, 0, SEEK_SET);
    length = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    buffer[length] = '\0';

    return expect(strcmp(buffer, expected) == 0, message);
}

static LT_Value read_one(const char* source){
    LT_Reader* reader = LT_Reader_new();
    LT_ReaderStream* stream = LT_ReaderStream_newForString(source);
    return LT_Reader_readObject(reader, stream);
}

static LT_Value LT__reader_error_tag = LT_NIL;

LT_DEFINE_PRIMITIVE(
    reader_error_handler,
    "reader-error-handler",
    "(condition)",
    "Throw caught reader condition for tests."
){
    LT_Value cursor = arguments;
    LT_Value condition;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, condition);
    LT_ARG_END(cursor);
    LT_throw(LT__reader_error_tag, condition);
}

static LT_Value read_one_catch_error(const char* source){
    LT_Value caught = LT_NIL;
    LT_Reader* reader = LT_Reader_new();
    LT_ReaderStream* stream = LT_ReaderStream_newForString(source);

    LT_CATCH(LT__reader_error_tag, caught, {
        LT_HANDLER_BIND(LT_Primitive_from_static(&reader_error_handler), {
            (void)LT_Reader_readObject(reader, stream);
        });
    });
    return caught;
}

static const char* condition_message_cstr(LT_Value condition){
    LT_Value message = LT_Object_slot_ref(condition, LT_Symbol_new("message"));
    return LT_String_value_cstr(LT_String_from_value(message));
}

static long long slot_fixnum_cstr(LT_Value object, const char* slot_name){
    LT_Value value = LT_Object_slot_ref(object, LT_Symbol_new((char*)slot_name));
    return LT_SmallInteger_value(value);
}

static int value_is_instance_of(LT_Value value, LT_Class* klass){
    return LT_Value_is_instance_of(value, (LT_Value)(uintptr_t)klass);
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

static int test_float_literal(void){
    LT_Value value = read_one("3.5");

    if (expect(LT_Float_p(value), "float tag")){
        return 1;
    }
    if (expect(LT_Value_class(value) == &LT_Float_class, "float class")){
        return 1;
    }

    return expect(
        LT_Float_value(value) == 3.5,
        "float value"
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

static int test_dispatch_character_single(void){
    LT_Value value = read_one("#\\A");

    if (expect(LT_Character_p(value), "dispatch character single type")){
        return 1;
    }
    return expect(
        LT_Character_value(value) == (uint32_t)'A',
        "dispatch character single value"
    );
}

static int test_dispatch_character_named(void){
    LT_Value value = read_one("#\\space");

    if (expect(LT_Character_p(value), "dispatch character named type")){
        return 1;
    }
    return expect(
        LT_Character_value(value) == (uint32_t)' ',
        "dispatch character named value"
    );
}

static int test_dispatch_character_unicode(void){
    LT_Value value = read_one("#\\u+03bb");

    if (expect(LT_Character_p(value), "dispatch character unicode type")){
        return 1;
    }
    return expect(
        LT_Character_value(value) == UINT32_C(0x03bb),
        "dispatch character unicode value"
    );
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

static int test_quasiquote_syntax(void){
    LT_Value value = read_one("`a");
    LT_Value tail;

    if (expect(LT_Pair_p(value), "quasiquote syntax returns list")){
        return 1;
    }
    if (expect(
        LT_Symbol_p(LT_car(value))
            && strcmp(
                LT_Symbol_name(LT_Symbol_from_value(LT_car(value))),
                "quasiquote"
            ) == 0,
        "quasiquote syntax head symbol"
    )){
        return 1;
    }

    tail = LT_cdr(value);
    if (expect(LT_Pair_p(tail), "quasiquote syntax has single argument")){
        return 1;
    }
    if (expect(
        LT_Symbol_p(LT_car(tail))
            && strcmp(
                LT_Symbol_name(LT_Symbol_from_value(LT_car(tail))),
                "a"
            ) == 0,
        "quasiquote syntax quoted value"
    )){
        return 1;
    }

    return expect(LT_cdr(tail) == LT_NIL, "quasiquote syntax argument list end");
}

static int test_unquote_syntax(void){
    LT_Value value = read_one(",a");
    LT_Value tail;

    if (expect(LT_Pair_p(value), "unquote syntax returns list")){
        return 1;
    }
    if (expect(
        LT_Symbol_p(LT_car(value))
            && strcmp(
                LT_Symbol_name(LT_Symbol_from_value(LT_car(value))),
                "unquote"
            ) == 0,
        "unquote syntax head symbol"
    )){
        return 1;
    }

    tail = LT_cdr(value);
    if (expect(LT_Pair_p(tail), "unquote syntax has single argument")){
        return 1;
    }
    if (expect(
        LT_Symbol_p(LT_car(tail))
            && strcmp(
                LT_Symbol_name(LT_Symbol_from_value(LT_car(tail))),
                "a"
            ) == 0,
        "unquote syntax argument value"
    )){
        return 1;
    }

    return expect(LT_cdr(tail) == LT_NIL, "unquote syntax argument list end");
}

static int test_unquote_splicing_syntax(void){
    LT_Value value = read_one(",@a");
    LT_Value tail;

    if (expect(LT_Pair_p(value), "unquote-splicing syntax returns list")){
        return 1;
    }
    if (expect(
        LT_Symbol_p(LT_car(value))
            && strcmp(
                LT_Symbol_name(LT_Symbol_from_value(LT_car(value))),
                "unquote-splicing"
            ) == 0,
        "unquote-splicing syntax head symbol"
    )){
        return 1;
    }

    tail = LT_cdr(value);
    if (expect(LT_Pair_p(tail), "unquote-splicing syntax has single argument")){
        return 1;
    }
    if (expect(
        LT_Symbol_p(LT_car(tail))
            && strcmp(
                LT_Symbol_name(LT_Symbol_from_value(LT_car(tail))),
                "a"
            ) == 0,
        "unquote-splicing syntax argument value"
    )){
        return 1;
    }

    return expect(
        LT_cdr(tail) == LT_NIL,
        "unquote-splicing syntax argument list end"
    );
}

static int test_quote_syntax_in_user_package_uses_listtalk_quote(void){
    LT_Value value = LT_NIL;

    LT_WITH_PACKAGE(LT_PACKAGE_LISTTALK_USER, {
        value = read_one("'a");
    });

    if (expect(LT_Pair_p(value), "quote syntax in user package returns list")){
        return 1;
    }
    if (expect(
        LT_Symbol_p(LT_car(value))
            && strcmp(
                LT_Symbol_name(LT_Symbol_from_value(LT_car(value))),
                "quote"
            ) == 0,
        "quote syntax in user package uses quote symbol"
    )){
        return 1;
    }
    return expect(
        LT_Symbol_package(LT_Symbol_from_value(LT_car(value))) == LT_PACKAGE_LISTTALK,
        "quote syntax in user package targets ListTalk quote symbol"
    );
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

static int test_reader_uses_thread_local_current_package(void){
    LT_Package* package = LT_Package_new("reader-test-package");
    LT_Value scoped_symbol = LT_NIL;
    LT_Value default_symbol;
    const char* name = "__reader_scoped_only__";

    LT_WITH_PACKAGE(package, {
        scoped_symbol = read_one(name);
    });
    default_symbol = read_one(name);

    if (expect(
        LT_Symbol_package(LT_Symbol_from_value(scoped_symbol)) == package,
        "reader interns unqualified symbol in current package"
    )){
        return 1;
    }
    return expect(
        LT_Symbol_package(LT_Symbol_from_value(default_symbol)) == LT_PACKAGE_LISTTALK,
        "reader restores previous package after LT_WITH_PACKAGE"
    );
}

static int test_symbol_print_omits_prefix_in_current_package(void){
    LT_Package* package = LT_Package_new("print-current");
    LT_Value symbol = LT_NIL;
    int failed = 0;

    LT_WITH_PACKAGE(package, {
        symbol = LT_Symbol_new("token");
        if (expect_symbol_print(
            symbol,
            "token",
            "symbol in current package prints without prefix"
        )){
            failed = 1;
        }
    });

    return failed;
}

static int test_symbol_print_omits_prefix_from_used_package_without_conflict(void){
    LT_Package* current = LT_Package_new("print-used-current");
    LT_Package* imported = LT_Package_new("print-used-imported");
    LT_Value symbol = LT_NIL;
    int failed = 0;

    LT_WITH_PACKAGE(imported, {
        symbol = LT_Symbol_new("token");
    });
    LT_Package_use_package(current, imported, NULL);

    LT_WITH_PACKAGE(current, {
        if (expect_symbol_print(
            symbol,
            "token",
            "used package symbol without local conflict prints without prefix"
        )){
            failed = 1;
        }
    });

    return failed;
}

static int test_symbol_print_keeps_prefix_for_used_package_conflict(void){
    LT_Package* current = LT_Package_new("print-conflict-current");
    LT_Package* imported = LT_Package_new("print-conflict-imported");
    LT_Value imported_symbol = LT_NIL;
    int failed = 0;

    LT_WITH_PACKAGE(imported, {
        imported_symbol = LT_Symbol_new("token");
    });
    LT_WITH_PACKAGE(current, {
        (void)LT_Symbol_new("token");
    });
    LT_Package_use_package(current, imported, NULL);

    LT_WITH_PACKAGE(current, {
        if (expect_symbol_print(
            imported_symbol,
            "print-conflict-imported:token",
            "used package symbol with local conflict keeps prefix"
        )){
            failed = 1;
        }
    });

    return failed;
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

static int test_dot_prefixed_tokens_inside_list(void){
    LT_Value value = read_one("(.slot .123)");
    LT_Value tail;
    LT_Value first;
    LT_Value second;

    if (expect(LT_Pair_p(value), "dot-prefixed list returns pair")){
        return 1;
    }

    first = LT_car(value);
    if (expect(LT_Pair_p(first), "dot-prefixed .slot expands in list")){
        return 1;
    }
    if (expect(
        strcmp(LT_Symbol_name(LT_Symbol_from_value(LT_car(first))), "%self-slot") == 0,
        "dot-prefixed .slot operator in list"
    )){
        return 1;
    }

    tail = LT_cdr(value);
    if (expect(LT_Pair_p(tail), "dot-prefixed second element exists")){
        return 1;
    }
    second = LT_car(tail);
    if (expect(LT_Float_p(second), "dot-prefixed .123 remains float in list")){
        return 1;
    }
    if (expect(
        LT_Float_value(second) == 0.123,
        "dot-prefixed .123 float value in list"
    )){
        return 1;
    }
    return expect(LT_cdr(tail) == LT_NIL, "dot-prefixed list terminates");
}

static int test_bare_dot_top_level_signals_error(void){
    LT_Value value = read_one_catch_error(".");

    if (expect(
        value_is_instance_of(value, &LT_Error_class),
        "bare dot signals error"
    )){
        return 1;
    }
    if (expect(LT_ReaderError_p(value), "bare dot signals reader error")){
        return 1;
    }
    return expect(
        strcmp(condition_message_cstr(value), "Unexpected dot") == 0,
        "bare dot top-level error message"
    );
}

static int test_incomplete_input_signals_specific_syntax_error(void){
    LT_Value value = read_one_catch_error("(a\n  (");

    if (expect(
        LT_IncompleteInputSyntaxError_p(value),
        "incomplete input signals incomplete syntax error"
    )){
        return 1;
    }
    if (expect(
        value_is_instance_of(value, &LT_ReaderError_class),
        "incomplete input is reader error"
    )){
        return 1;
    }
    if (expect(
        value_is_instance_of(value, &LT_Error_class),
        "incomplete input is error condition"
    )){
        return 1;
    }
    if (expect(
        strcmp(condition_message_cstr(value), "Unterminated list") == 0,
        "incomplete input message"
    )){
        return 1;
    }
    if (expect(slot_fixnum_cstr(value, "line") == 2, "incomplete input line")){
        return 1;
    }
    if (expect(slot_fixnum_cstr(value, "column") == 3, "incomplete input column")){
        return 1;
    }
    return expect(
        slot_fixnum_cstr(value, "nesting-depth") == 2,
        "incomplete input nesting depth"
    );
}

static int test_reader_tracks_line_column_and_nesting_depth(void){
    LT_Reader* reader = LT_Reader_new();
    LT_ReaderStream* stream = LT_ReaderStream_newForString("(a\n  (b))");
    LT_Value value = LT_Reader_readObject(reader, stream);

    if (expect(LT_Pair_p(value), "tracked reader still returns parsed object")){
        return 1;
    }
    if (expect(slot_fixnum_cstr((LT_Value)(uintptr_t)reader, "line") == 2, "reader line")){
        return 1;
    }
    if (expect(
        slot_fixnum_cstr((LT_Value)(uintptr_t)reader, "column") == 6,
        "reader column"
    )){
        return 1;
    }
    return expect(
        slot_fixnum_cstr((LT_Value)(uintptr_t)reader, "nesting-depth") == 0,
        "reader nesting depth"
    );
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
    LT__reader_error_tag = LT_Symbol_new("reader-test-error");

    failures += test_symbol();
    failures += test_string();
    failures += test_nil_list();
    failures += test_proper_list();
    failures += test_dotted_pair();
    failures += test_comment_and_whitespace();
    failures += test_fixnum_literal();
    failures += test_negative_fixnum_literal();
    failures += test_float_literal();
    failures += test_symbol_not_number();
    failures += test_dispatch_boolean_true();
    failures += test_dispatch_boolean_false();
    failures += test_dispatch_nil();
    failures += test_dispatch_nil_short();
    failures += test_dispatch_character_single();
    failures += test_dispatch_character_named();
    failures += test_dispatch_character_unicode();
    failures += test_dispatch_bang_comment();
    failures += test_quote_syntax();
    failures += test_quasiquote_syntax();
    failures += test_unquote_syntax();
    failures += test_unquote_splicing_syntax();
    failures += test_quote_syntax_in_user_package_uses_listtalk_quote();
    failures += test_symbol_package_interning();
    failures += test_reader_uses_thread_local_current_package();
    failures += test_symbol_print_omits_prefix_in_current_package();
    failures += test_symbol_print_omits_prefix_from_used_package_without_conflict();
    failures += test_symbol_print_keeps_prefix_for_used_package_conflict();
    failures += test_package_prefixed_symbol();
    failures += test_package_prefix_last_colon_split();
    failures += test_keyword_prefix_symbol();
    failures += test_bracket_unary_send_syntax();
    failures += test_bracket_keyword_send_syntax();
    failures += test_slot_accessor_syntax();
    failures += test_dot_prefixed_tokens_inside_list();
    failures += test_bare_dot_top_level_signals_error();
    failures += test_incomplete_input_signals_specific_syntax_error();
    failures += test_reader_tracks_line_column_and_nesting_depth();
    failures += test_vector_literal_empty();
    failures += test_vector_literal_values();

    if (failures == 0){
        puts("reader tests passed");
        return 0;
    }

    fprintf(stderr, "%d reader test(s) failed\n", failures);
    return 1;
}
