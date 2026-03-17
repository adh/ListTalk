/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>

#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/error.h>

#include <string.h>

typedef struct LT_QuasiquoteResult_s {
    LT_Value value;
    int splice;
} LT_QuasiquoteResult;

static int symbol_is_listtalk_name(LT_Value value, const char* name){
    if (!LT_Symbol_p(value)){
        return 0;
    }
    if (LT_Symbol_package(LT_Symbol_from_value(value)) != LT_PACKAGE_LISTTALK){
        return 0;
    }
    return strcmp(LT_Symbol_name(LT_Symbol_from_value(value)), name) == 0;
}

static LT_Value quasiquote_single_argument(LT_Value form, const char* operator_name){
    LT_Value tail = LT_cdr(form);
    LT_Value argument;

    if (!LT_Pair_p(tail)){
        LT_error(
            "Operator expects exactly one argument",
            "operator", LT_Symbol_new((char*)operator_name),
            NULL
        );
    }

    argument = LT_car(tail);
    if (LT_cdr(tail) != LT_NIL){
        LT_error(
            "Operator expects exactly one argument",
            "operator", LT_Symbol_new((char*)operator_name),
            NULL
        );
    }

    return argument;
}

static void quasiquote_append_splice(
    LT_ListBuilder* builder,
    LT_Value splice_value
){
    LT_Value cursor = splice_value;

    while (cursor != LT_NIL){
        if (!LT_Pair_p(cursor)){
            LT_error("unquote-splicing expects proper list");
        }
        LT_ListBuilder_append(builder, LT_car(cursor));
        cursor = LT_cdr(cursor);
    }
}

static LT_QuasiquoteResult quasiquote_walk(
    LT_Value expression,
    LT_Environment* environment,
    size_t quasiquote_depth,
    int list_context
){
    LT_QuasiquoteResult result;

    result.value = expression;
    result.splice = 0;

    if (!LT_Pair_p(expression)){
        return result;
    }

    if (symbol_is_listtalk_name(LT_car(expression), "unquote")){
        LT_Value argument = quasiquote_single_argument(expression, "unquote");

        if (quasiquote_depth == 1){
            result.value = LT_eval(argument, environment, NULL);
            return result;
        }

        result.value = LT_list(
            LT_car(expression),
            quasiquote_walk(argument, environment, quasiquote_depth - 1, 0).value,
            LT_INVALID
        );
        return result;
    }

    if (symbol_is_listtalk_name(LT_car(expression), "unquote-splicing")){
        LT_Value argument = quasiquote_single_argument(
            expression,
            "unquote-splicing"
        );

        if (quasiquote_depth == 1){
            if (!list_context){
                LT_error("unquote-splicing outside list context");
            }
            result.value = LT_eval(argument, environment, NULL);
            result.splice = 1;
            return result;
        }

        result.value = LT_list(
            LT_car(expression),
            quasiquote_walk(argument, environment, quasiquote_depth - 1, 0).value,
            LT_INVALID
        );
        return result;
    }

    if (symbol_is_listtalk_name(LT_car(expression), "quasiquote")){
        LT_Value argument = quasiquote_single_argument(expression, "quasiquote");

        result.value = LT_list(
            LT_car(expression),
            quasiquote_walk(argument, environment, quasiquote_depth + 1, 0).value,
            LT_INVALID
        );
        return result;
    }

    {
        LT_ListBuilder* builder = LT_ListBuilder_new();
        LT_Value cursor = expression;

        while (LT_Pair_p(cursor)){
            LT_QuasiquoteResult item = quasiquote_walk(
                LT_car(cursor),
                environment,
                quasiquote_depth,
                1
            );

            if (item.splice){
                quasiquote_append_splice(builder, item.value);
            } else {
                LT_ListBuilder_append(builder, item.value);
            }

            cursor = LT_cdr(cursor);
        }

        if (cursor == LT_NIL){
            result.value = LT_ListBuilder_value(builder);
            return result;
        }

        result = quasiquote_walk(cursor, environment, quasiquote_depth, 0);
        if (result.splice){
            LT_error("unquote-splicing outside list context");
        }

        result.value = LT_ListBuilder_valueWithRest(builder, result.value);
        return result;
    }
}

LT_Value LT_quasiquote(LT_Value expression, LT_Environment* environment){
    LT_QuasiquoteResult result;

    if (environment == NULL){
        LT_error("quasiquote expects environment");
    }

    result = quasiquote_walk(expression, environment, 1, 0);
    if (result.splice){
        LT_error("unquote-splicing outside list context");
    }
    return result.value;
}
