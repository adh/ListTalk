/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>

#include <stdio.h>
#include <string.h>

static int expect(int condition, const char* message){
    if (!condition){
        fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

static int string_equals(LT_Value value, const char* expected){
    return LT_String_p(value)
        && strcmp(
            LT_String_value_cstr(LT_String_from_value(value)),
            expected
        ) == 0;
}

static int test_utf8_match_and_captures(void){
    LT_RegularExpression* expression = LT_RegularExpression_new(
        LT_String_new_cstr("(\\p{L}+)(?:-(\\d+))?")
    );
    LT_Value value = LT_RegularExpression_match(
        expression,
        LT_String_new_cstr("🔥 žluť-42")
    );
    LT_RegularExpressionMatch* match;
    size_t from;
    size_t to;
    int failures = 0;

    failures += expect(
        LT_RegularExpressionMatch_p(value),
        "matching returns RegularExpressionMatch"
    );
    if (!LT_RegularExpressionMatch_p(value)){
        return failures;
    }
    match = LT_RegularExpressionMatch_from_value(value);
    failures += expect(
        LT_RegularExpressionMatch_capture_count(match) == 3,
        "capture count includes complete match"
    );
    failures += expect(
        string_equals(LT_RegularExpressionMatch_capture(match, 0), "žluť-42"),
        "capture zero is complete match"
    );
    failures += expect(
        string_equals(LT_RegularExpressionMatch_capture(match, 1), "žluť"),
        "Unicode capture is preserved"
    );
    failures += expect(
        string_equals(LT_RegularExpressionMatch_capture(match, 2), "42"),
        "second capture is available"
    );
    failures += expect(
        LT_RegularExpressionMatch_range(match, 0, &from, &to)
            && from == 2
            && to == 9,
        "match range uses Unicode code-point indices"
    );
    return failures;
}

static int test_optional_capture_and_no_match(void){
    LT_RegularExpression* expression = LT_RegularExpression_new(
        LT_String_new_cstr("(\\p{L}+)(?:-(\\d+))?")
    );
    LT_Value value = LT_RegularExpression_match(
        expression,
        LT_String_new_cstr("žluť")
    );
    LT_RegularExpressionMatch* match =
        LT_RegularExpressionMatch_from_value(value);
    size_t from = 99;
    size_t to = 99;
    int failures = 0;

    failures += expect(
        LT_RegularExpressionMatch_capture(match, 2) == LT_FALSE,
        "unmatched optional capture is false"
    );
    failures += expect(
        !LT_RegularExpressionMatch_range(match, 2, &from, &to),
        "unmatched optional capture has no range"
    );
    failures += expect(
        LT_RegularExpression_match(
            LT_RegularExpression_new(LT_String_new_cstr("\\d+")),
            LT_String_new_cstr("no digits")
        ) == LT_FALSE,
        "no match returns false"
    );
    return failures;
}

static int test_pattern_value_semantics(void){
    LT_RegularExpression* left = LT_RegularExpression_new(
        LT_String_new_cstr("a+")
    );
    LT_RegularExpression* right = LT_RegularExpression_new(
        LT_String_new_cstr("a+")
    );
    LT_Value left_value = (LT_Value)(uintptr_t)left;
    LT_Value right_value = (LT_Value)(uintptr_t)right;
    int failures = 0;

    failures += expect(
        LT_Value_equal_p(left_value, right_value),
        "equal source patterns produce equal expressions"
    );
    failures += expect(
        LT_Value_hash(left_value) == LT_Value_hash(right_value),
        "equal expressions have equal hashes"
    );
    return failures;
}

int main(void){
    int failures = 0;

    LT_INIT();
    failures += test_utf8_match_and_captures();
    failures += test_optional_capture_and_no_match();
    failures += test_pattern_value_semantics();

    if (failures == 0){
        puts("regular expression tests passed");
        return 0;
    }
    fprintf(stderr, "%d regular expression test(s) failed\n", failures);
    return 1;
}
