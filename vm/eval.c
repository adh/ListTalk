/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>

#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/error.h>

static LT_Value eval_form(LT_Value expression, LT_Environment* environment);

static LT_Value eval_list_items(LT_Value list, LT_Environment* environment){
    LT_ListBuilder* builder = LT_ListBuilder_new();
    LT_Value cursor = list;

    while (cursor != LT_VALUE_NIL){
        if (!LT_Value_is_pair(cursor)){
            LT_error("Application expects a proper list of arguments");
        }
        LT_ListBuilder_append(
            builder,
            eval_form(LT_car(cursor), environment)
        );
        cursor = LT_cdr(cursor);
    }

    return LT_ListBuilder_value(builder);
}

static LT_Value apply_form(LT_Value operator,
                           LT_Value argument_expressions,
                           LT_Environment* environment){
    LT_Value evaluated_operator = eval_form(operator, environment);
    LT_Value evaluated_arguments;

    if (!LT_Value_is_primitive(evaluated_operator)){
        LT_error("Tried to apply non-primitive value");
    }

    evaluated_arguments = eval_list_items(argument_expressions, environment);
    return LT_Primitive_call(evaluated_operator, evaluated_arguments);
}

static LT_Value eval_symbol(LT_Value symbol, LT_Environment* environment){
    LT_Value value;

    if (!LT_Environment_lookup(environment, symbol, &value, NULL)){
        LT_error("Unbound symbol");
    }
    return value;
}

static LT_Value eval_form(LT_Value expression, LT_Environment* environment){
    if (LT_Value_is_symbol(expression)){
        return eval_symbol(expression, environment);
    }

    if (LT_Value_is_pair(expression)){
        return apply_form(
            LT_car(expression),
            LT_cdr(expression),
            environment
        );
    }

    return expression;
}

LT_Value LT_eval(LT_Value expression, LT_Environment* environment){
    if (environment == NULL){
        LT_error("Evaluator expects environment");
    }
    return eval_form(expression, environment);
}
