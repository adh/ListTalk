/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/RegularExpression.h>
#include <ListTalk/classes/RegularExpressionMatch.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/classes/Class.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/utils.h>

#include <gc.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct LT_RegularExpression_s {
    LT_Object base;
    LT_String* pattern;
    pcre2_code* code;
    size_t capture_count;
};

struct LT_RegularExpressionMatch_s {
    LT_Object base;
    LT_RegularExpression* expression;
    LT_String* subject;
    size_t capture_count;
    PCRE2_SIZE ranges[];
};

static size_t codepoint_index_for_byte_offset(LT_String* string,
                                              size_t byte_offset){
    const char* start = LT_String_value_cstr(string);
    const char* cursor = start;
    const char* target = start + byte_offset;
    size_t index = 0;

    while (cursor < target){
        cursor = LT_String_utf8_next(cursor);
        index++;
    }
    return index;
}

static void RegularExpression_finalizer(void* object, void* data){
    LT_RegularExpression* expression = object;
    (void)data;

    if (expression->code != NULL){
        pcre2_code_free(expression->code);
        expression->code = NULL;
    }
}

static size_t RegularExpression_hash(LT_Value value){
    LT_RegularExpression* expression = LT_RegularExpression_from_value(value);
    return LT_Value_hash((LT_Value)(uintptr_t)expression->pattern);
}

static int RegularExpression_equal_p(LT_Value left, LT_Value right){
    LT_RegularExpression* left_expression;
    LT_RegularExpression* right_expression;

    if (!LT_RegularExpression_p(right)){
        return 0;
    }
    left_expression = LT_RegularExpression_from_value(left);
    right_expression = LT_RegularExpression_from_value(right);
    return LT_Value_equal_p(
        (LT_Value)(uintptr_t)left_expression->pattern,
        (LT_Value)(uintptr_t)right_expression->pattern
    );
}

static void RegularExpression_debugPrintOn(LT_Value value, FILE* stream){
    LT_RegularExpression* expression = LT_RegularExpression_from_value(value);
    fprintf(
        stream,
        "#<RegularExpression %s>",
        LT_String_value_cstr(expression->pattern)
    );
}

static void RegularExpressionMatch_debugPrintOn(LT_Value value, FILE* stream){
    LT_RegularExpressionMatch* match =
        LT_RegularExpressionMatch_from_value(value);
    LT_Value capture = LT_RegularExpressionMatch_capture(match, 0);

    if (capture == LT_FALSE){
        fputs("#<RegularExpressionMatch unmatched>", stream);
    } else {
        fprintf(
            stream,
            "#<RegularExpressionMatch %s>",
            LT_String_value_cstr(LT_String_from_value(capture))
        );
    }
}

LT_DEFINE_PRIMITIVE(
    regular_expression_class_method_from_string,
    "RegularExpression class>>fromString:",
    "(self pattern)",
    "Compile pattern as a UTF-8 regular expression."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* pattern;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, pattern, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_RegularExpression_class){
        LT_error("fromString: class method is only supported on RegularExpression");
    }
    return (LT_Value)(uintptr_t)LT_RegularExpression_new(pattern);
}

LT_DEFINE_PRIMITIVE(
    regular_expression_method_pattern,
    "RegularExpression>>pattern",
    "(self)",
    "Return the source pattern."
){
    LT_Value cursor = arguments;
    LT_RegularExpression* expression;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(
        cursor,
        expression,
        LT_RegularExpression*,
        LT_RegularExpression_from_value
    );
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_RegularExpression_pattern(expression);
}

LT_DEFINE_PRIMITIVE(
    regular_expression_method_match,
    "RegularExpression>>match:",
    "(self subject)",
    "Return the first match in subject, or false when there is no match."
){
    LT_Value cursor = arguments;
    LT_RegularExpression* expression;
    LT_String* subject;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(
        cursor,
        expression,
        LT_RegularExpression*,
        LT_RegularExpression_from_value
    );
    LT_GENERIC_ARG(cursor, subject, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);
    return LT_RegularExpression_match(expression, subject);
}

LT_DEFINE_PRIMITIVE(
    regular_expression_method_substitute_with,
    "RegularExpression>>substitute:with:",
    "(self subject replacement)",
    "Return subject with every match replaced, expanding capture references."
){
    LT_Value cursor = arguments;
    LT_RegularExpression* expression;
    LT_String* subject;
    LT_String* replacement;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(
        cursor,
        expression,
        LT_RegularExpression*,
        LT_RegularExpression_from_value
    );
    LT_GENERIC_ARG(cursor, subject, LT_String*, LT_String_from_value);
    LT_GENERIC_ARG(cursor, replacement, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_RegularExpression_substitute(
        expression,
        subject,
        replacement
    );
}

LT_DEFINE_PRIMITIVE(
    regular_expression_match_method_expression,
    "RegularExpressionMatch>>regularExpression",
    "(self)",
    "Return the regular expression that produced the match."
){
    LT_Value cursor = arguments;
    LT_RegularExpressionMatch* match;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(
        cursor,
        match,
        LT_RegularExpressionMatch*,
        LT_RegularExpressionMatch_from_value
    );
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_RegularExpressionMatch_expression(match);
}

LT_DEFINE_PRIMITIVE(
    regular_expression_match_method_subject,
    "RegularExpressionMatch>>subject",
    "(self)",
    "Return the matched subject string."
){
    LT_Value cursor = arguments;
    LT_RegularExpressionMatch* match;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(
        cursor,
        match,
        LT_RegularExpressionMatch*,
        LT_RegularExpressionMatch_from_value
    );
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_RegularExpressionMatch_subject(match);
}

LT_DEFINE_PRIMITIVE(
    regular_expression_match_method_capture_count,
    "RegularExpressionMatch>>captureCount",
    "(self)",
    "Return the number of captures, including the complete match."
){
    LT_Value cursor = arguments;
    LT_RegularExpressionMatch* match;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(
        cursor,
        match,
        LT_RegularExpressionMatch*,
        LT_RegularExpressionMatch_from_value
    );
    LT_ARG_END(cursor);
    return LT_Number_smallinteger_from_size(
        LT_RegularExpressionMatch_capture_count(match),
        "Regular expression capture count does not fit fixnum"
    );
}

static size_t match_capture_index(LT_Value value){
    return LT_Number_nonnegative_size_from_integer(
        value,
        "Regular expression capture index out of bounds",
        "Regular expression capture index out of bounds"
    );
}

LT_DEFINE_PRIMITIVE(
    regular_expression_match_method_at,
    "RegularExpressionMatch>>at:",
    "(self index)",
    "Return a captured substring, or false for an unmatched optional capture."
){
    LT_Value cursor = arguments;
    LT_RegularExpressionMatch* match;
    LT_Value index;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(
        cursor,
        match,
        LT_RegularExpressionMatch*,
        LT_RegularExpressionMatch_from_value
    );
    LT_OBJECT_ARG(cursor, index);
    LT_ARG_END(cursor);
    return LT_RegularExpressionMatch_capture(match, match_capture_index(index));
}

static LT_Value match_range_endpoint(LT_RegularExpressionMatch* match,
                                     LT_Value index,
                                     int want_end){
    size_t from;
    size_t to;

    if (!LT_RegularExpressionMatch_range(
        match,
        match_capture_index(index),
        &from,
        &to
    )){
        return LT_FALSE;
    }
    return LT_Number_smallinteger_from_size(
        want_end ? to : from,
        "Regular expression match index does not fit fixnum"
    );
}

LT_DEFINE_PRIMITIVE(
    regular_expression_match_method_from,
    "RegularExpressionMatch>>from:",
    "(self index)",
    "Return the capture start as a Unicode code-point index, or false."
){
    LT_Value cursor = arguments;
    LT_RegularExpressionMatch* match;
    LT_Value index;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, match, LT_RegularExpressionMatch*,
                   LT_RegularExpressionMatch_from_value);
    LT_OBJECT_ARG(cursor, index);
    LT_ARG_END(cursor);
    return match_range_endpoint(match, index, 0);
}

LT_DEFINE_PRIMITIVE(
    regular_expression_match_method_to,
    "RegularExpressionMatch>>to:",
    "(self index)",
    "Return the exclusive capture end as a Unicode code-point index, or false."
){
    LT_Value cursor = arguments;
    LT_RegularExpressionMatch* match;
    LT_Value index;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, match, LT_RegularExpressionMatch*,
                   LT_RegularExpressionMatch_from_value);
    LT_OBJECT_ARG(cursor, index);
    LT_ARG_END(cursor);
    return match_range_endpoint(match, index, 1);
}

static LT_Method_Descriptor RegularExpression_methods[] = {
    {"pattern", &regular_expression_method_pattern},
    {"match:", &regular_expression_method_match},
    {"substitute:with:", &regular_expression_method_substitute_with},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor RegularExpression_class_methods[] = {
    {"fromString:", &regular_expression_class_method_from_string},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor RegularExpressionMatch_methods[] = {
    {"regularExpression", &regular_expression_match_method_expression},
    {"subject", &regular_expression_match_method_subject},
    {"captureCount", &regular_expression_match_method_capture_count},
    {"at:", &regular_expression_match_method_at},
    {"from:", &regular_expression_match_method_from},
    {"to:", &regular_expression_match_method_to},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_RegularExpression) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "RegularExpression",
    .documentation = "Immutable compiled UTF-8 regular expression.",
    .instance_size = sizeof(LT_RegularExpression),
    .class_flags = LT_CLASS_FLAG_IMMUTABLE | LT_CLASS_FLAG_SCALAR,
    .hash = RegularExpression_hash,
    .equal_p = RegularExpression_equal_p,
    .debugPrintOn = RegularExpression_debugPrintOn,
    .methods = RegularExpression_methods,
    .class_methods = RegularExpression_class_methods,
};

LT_DEFINE_CLASS(LT_RegularExpressionMatch) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "RegularExpressionMatch",
    .documentation = "Immutable result of matching a regular expression.",
    .instance_size = sizeof(LT_RegularExpressionMatch),
    .class_flags = LT_CLASS_FLAG_IMMUTABLE | LT_CLASS_FLAG_FINAL,
    .debugPrintOn = RegularExpressionMatch_debugPrintOn,
    .methods = RegularExpressionMatch_methods,
};

LT_RegularExpression* LT_RegularExpression_new(LT_String* pattern){
    LT_RegularExpression* expression;
    int error_code;
    PCRE2_SIZE error_offset;
    uint32_t capture_count;

    expression = LT_Class_ALLOC(LT_RegularExpression);
    expression->pattern = pattern;
    expression->code = pcre2_compile(
        (PCRE2_SPTR)LT_String_value_cstr(pattern),
        (PCRE2_SIZE)LT_String_byte_length(pattern),
        PCRE2_UTF | PCRE2_UCP,
        &error_code,
        &error_offset,
        NULL
    );
    if (expression->code == NULL){
        PCRE2_UCHAR message[256];
        int result = pcre2_get_error_message(
            error_code,
            message,
            sizeof(message)
        );

        if (result < 0){
            LT_error("Unable to compile regular expression");
        }
        LT_error("Invalid regular expression at byte %zu: %s",
                 (size_t)error_offset, (char*)message);
    }
    if (pcre2_pattern_info(
        expression->code,
        PCRE2_INFO_CAPTURECOUNT,
        &capture_count
    ) != 0){
        pcre2_code_free(expression->code);
        expression->code = NULL;
        LT_error("Unable to inspect compiled regular expression");
    }
    expression->capture_count = (size_t)capture_count + 1;
    GC_register_finalizer(
        expression,
        RegularExpression_finalizer,
        NULL,
        NULL,
        NULL
    );
    return expression;
}

LT_String* LT_RegularExpression_pattern(LT_RegularExpression* expression){
    return expression->pattern;
}

LT_Value LT_RegularExpression_match(LT_RegularExpression* expression,
                                    LT_String* subject){
    pcre2_match_data* data = pcre2_match_data_create_from_pattern(
        expression->code,
        NULL
    );
    int result;
    LT_RegularExpressionMatch* match;
    PCRE2_SIZE* source_ranges;
    size_t range_count = expression->capture_count * 2;
    size_t i;

    if (data == NULL){
        LT_error("Unable to allocate regular expression match data");
    }
    result = pcre2_match(
        expression->code,
        (PCRE2_SPTR)LT_String_value_cstr(subject),
        (PCRE2_SIZE)LT_String_byte_length(subject),
        0,
        PCRE2_NO_UTF_CHECK,
        data,
        NULL
    );
    if (result == PCRE2_ERROR_NOMATCH){
        pcre2_match_data_free(data);
        return LT_FALSE;
    }
    if (result < 0){
        pcre2_match_data_free(data);
        LT_error("Regular expression matching failed with PCRE2 error %d", result);
    }

    match = LT_Class_ALLOC_FLEXIBLE(
        LT_RegularExpressionMatch,
        range_count * sizeof(PCRE2_SIZE)
    );
    match->expression = expression;
    match->subject = subject;
    match->capture_count = expression->capture_count;
    source_ranges = pcre2_get_ovector_pointer(data);
    for (i = 0; i < range_count; i++){
        match->ranges[i] = source_ranges[i];
    }
    pcre2_match_data_free(data);
    return (LT_Value)(uintptr_t)match;
}

static void RegularExpression_list_callback(LT_String* substring, void* baton){
    LT_ListBuilder_append((LT_ListBuilder*)baton, (LT_Value)(uintptr_t)substring);
}

void LT_RegularExpression_splitDo(
    LT_RegularExpression* expression,
    LT_String* subject,
    LT_String_SubstringCallback callback,
    void* baton
){
    const char* bytes = LT_String_value_cstr(subject);
    PCRE2_SIZE length = (PCRE2_SIZE)LT_String_byte_length(subject);
    PCRE2_SIZE field_start = 0;
    PCRE2_SIZE search_start = 0;
    pcre2_match_data* data = pcre2_match_data_create_from_pattern(
        expression->code,
        NULL
    );

    if (data == NULL){
        LT_error("Unable to allocate regular expression match data");
    }
    while (search_start <= length){
        int result = pcre2_match(
            expression->code,
            (PCRE2_SPTR)bytes,
            length,
            search_start,
            PCRE2_NO_UTF_CHECK,
            data,
            NULL
        );
        PCRE2_SIZE* ranges;
        PCRE2_SIZE from;
        PCRE2_SIZE to;

        if (result == PCRE2_ERROR_NOMATCH){
            break;
        }
        if (result < 0){
            pcre2_match_data_free(data);
            LT_error("Regular expression matching failed with PCRE2 error %d", result);
        }
        ranges = pcre2_get_ovector_pointer(data);
        from = ranges[0];
        to = ranges[1];
        callback(
            LT_String_new((char*)bytes + field_start, (size_t)(from - field_start)),
            baton
        );
        field_start = to;
        if (to > from){
            search_start = to;
        } else if (to < length){
            search_start = (PCRE2_SIZE)(LT_String_utf8_next(bytes + to) - bytes);
        } else {
            search_start = length + 1;
        }
    }
    callback(
        LT_String_new((char*)bytes + field_start, (size_t)(length - field_start)),
        baton
    );
    pcre2_match_data_free(data);
}

LT_Value LT_RegularExpression_split(LT_RegularExpression* expression,
                                    LT_String* subject){
    LT_ListBuilder* builder = LT_ListBuilder_new();

    LT_RegularExpression_splitDo(
        expression,
        subject,
        RegularExpression_list_callback,
        builder
    );
    return LT_ListBuilder_value(builder);
}

LT_String* LT_RegularExpression_substitute(
    LT_RegularExpression* expression,
    LT_String* subject,
    LT_String* replacement
){
    PCRE2_SIZE capacity = (PCRE2_SIZE)(
        LT_String_byte_length(subject)
        + LT_String_byte_length(replacement)
        + 1
    );
    PCRE2_UCHAR* output;
    PCRE2_SIZE output_length;
    int result;
    uint32_t options = PCRE2_SUBSTITUTE_GLOBAL
        | PCRE2_SUBSTITUTE_OVERFLOW_LENGTH
        | PCRE2_SUBSTITUTE_UNSET_EMPTY
        | PCRE2_NO_UTF_CHECK;

    if (capacity == 0){
        capacity = 1;
    }
    for (;;){
        output = GC_MALLOC_ATOMIC((size_t)capacity);
        if (output == NULL){
            LT_error("Unable to allocate regular expression substitution buffer");
        }
        output_length = capacity;
        result = pcre2_substitute(
            expression->code,
            (PCRE2_SPTR)LT_String_value_cstr(subject),
            (PCRE2_SIZE)LT_String_byte_length(subject),
            0,
            options,
            NULL,
            NULL,
            (PCRE2_SPTR)LT_String_value_cstr(replacement),
            (PCRE2_SIZE)LT_String_byte_length(replacement),
            output,
            &output_length
        );
        if (result != PCRE2_ERROR_NOMEMORY){
            break;
        }
        if (output_length <= capacity){
            LT_error("Unable to size regular expression substitution output");
        }
        capacity = output_length;
    }
    if (result < 0){
        LT_error(
            "Regular expression substitution failed with PCRE2 error %d",
            result
        );
    }
    return LT_String_new((char*)output, (size_t)output_length);
}

LT_RegularExpression* LT_RegularExpressionMatch_expression(
    LT_RegularExpressionMatch* match
){
    return match->expression;
}

LT_String* LT_RegularExpressionMatch_subject(LT_RegularExpressionMatch* match){
    return match->subject;
}

size_t LT_RegularExpressionMatch_capture_count(
    LT_RegularExpressionMatch* match
){
    return match->capture_count;
}

static void require_capture_index(LT_RegularExpressionMatch* match,
                                  size_t index){
    if (index >= match->capture_count){
        LT_error("Regular expression capture index out of bounds");
    }
}

LT_Value LT_RegularExpressionMatch_capture(LT_RegularExpressionMatch* match,
                                           size_t index){
    PCRE2_SIZE from;
    PCRE2_SIZE to;

    require_capture_index(match, index);
    from = match->ranges[index * 2];
    to = match->ranges[index * 2 + 1];
    if (from == PCRE2_UNSET || to == PCRE2_UNSET){
        return LT_FALSE;
    }
    return (LT_Value)(uintptr_t)LT_String_new(
        (char*)LT_String_value_cstr(match->subject) + from,
        (size_t)(to - from)
    );
}

int LT_RegularExpressionMatch_range(LT_RegularExpressionMatch* match,
                                    size_t index,
                                    size_t* from_out,
                                    size_t* to_out){
    PCRE2_SIZE from;
    PCRE2_SIZE to;

    require_capture_index(match, index);
    from = match->ranges[index * 2];
    to = match->ranges[index * 2 + 1];
    if (from == PCRE2_UNSET || to == PCRE2_UNSET){
        return 0;
    }
    if (from_out != NULL){
        *from_out = codepoint_index_for_byte_offset(match->subject, from);
    }
    if (to_out != NULL){
        *to_out = codepoint_index_for_byte_offset(match->subject, to);
    }
    return 1;
}
