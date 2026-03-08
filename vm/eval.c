/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>

#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Closure.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Macro.h>
#include <ListTalk/classes/SpecialForm.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/error.h>

static LT_Value eval_form(LT_Value expression, LT_Environment* environment);

static LT_Value eval_list_items(LT_Value list, LT_Environment* environment){
    LT_ListBuilder* builder = LT_ListBuilder_new();
    LT_Value cursor = list;

    while (cursor != LT_NIL){
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

static void bind_closure_parameters(LT_Value parameters,
                                    LT_Value arguments,
                                    LT_Environment* target_environment){
    LT_Value parameter_cursor = parameters;
    LT_Value argument_cursor = arguments;

    while (parameter_cursor != LT_NIL && argument_cursor != LT_NIL){
        LT_Value parameter;

        if (!LT_Value_is_pair(parameter_cursor)
            || !LT_Value_is_pair(argument_cursor)){
            LT_error("Closure application expects proper argument lists");
        }

        parameter = LT_car(parameter_cursor);
        if (!LT_Value_is_symbol(parameter)){
            LT_error("Closure parameter must be symbol");
        }

        LT_Environment_bind(
            target_environment,
            parameter,
            LT_car(argument_cursor),
            0
        );

        parameter_cursor = LT_cdr(parameter_cursor);
        argument_cursor = LT_cdr(argument_cursor);
    }

    if (parameter_cursor != LT_NIL || argument_cursor != LT_NIL){
        LT_error("Closure arity mismatch");
    }
}

static LT_Value eval_sequence(LT_Value body, LT_Environment* environment){
    LT_Value cursor = body;
    LT_Value result = LT_NIL;

    while (cursor != LT_NIL){
        if (!LT_Value_is_pair(cursor)){
            LT_error("Closure body expects proper list of forms");
        }
        result = eval_form(LT_car(cursor), environment);
        cursor = LT_cdr(cursor);
    }

    return result;
}

static LT_Value apply_closure(LT_Value closure_value, LT_Value evaluated_arguments){
    LT_Closure* closure = LT_Closure_from_object(closure_value);
    LT_Environment* application_environment = LT_Environment_new(
        LT_Closure_environment(closure)
    );

    bind_closure_parameters(
        LT_Closure_parameters(closure),
        evaluated_arguments,
        application_environment
    );

    return eval_sequence(LT_Closure_body(closure), application_environment);
}

static LT_Value apply_callable(LT_Value callable,
                               LT_Value argument_expressions,
                               LT_Environment* environment,
                               int evaluate_arguments){
    LT_Value arguments = argument_expressions;

    if (LT_Value_is_special_form(callable)){
        return LT_SpecialForm_apply(callable, argument_expressions, environment);
    }
    if (evaluate_arguments){
        arguments = eval_list_items(argument_expressions, environment);
    }
    if (LT_Value_is_primitive(callable)){
        return LT_Primitive_call(callable, arguments);
    }
    if (LT_Value_is_closure(callable)){
        return apply_closure(callable, arguments);
    }

    LT_error("Tried to apply non-callable value");
    return LT_NIL;
}

static LT_Value apply_form(LT_Value operator,
                           LT_Value argument_expressions,
                           LT_Environment* environment){
    LT_Value evaluated_operator = eval_form(operator, environment);
    LT_Value expansion;
    LT_Value implementation;

    if (LT_Value_is_macro(evaluated_operator)){
        implementation = LT_Macro_callable(
            LT_Macro_from_object(evaluated_operator)
        );
        if (!LT_Value_is_primitive(implementation)
            && !LT_Value_is_closure(implementation)){
            LT_error("Macro implementation must be primitive or closure");
        }
        expansion = apply_callable(
            implementation,
            argument_expressions,
            environment,
            0
        );
        return eval_form(expansion, environment);
    }

    return apply_callable(
        evaluated_operator,
        argument_expressions,
        environment,
        1
    );
}

static LT_Value eval_symbol(LT_Value symbol, LT_Environment* environment){
    LT_Value value;

    if (!LT_Environment_lookup(environment, symbol, &value, NULL)){
        if (LT_Symbol_package(LT_Symbol_from_object(symbol))
            == LT_PACKAGE_KEYWORD){
            return symbol;
        }
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
