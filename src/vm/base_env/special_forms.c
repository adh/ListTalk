/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Closure.h>
#include <ListTalk/classes/Macro.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/error.h>

static LT_Value special_form_quote(LT_Value arguments,
                                   LT_Environment* environment){
    LT_Value cursor = arguments;
    LT_Value value;
    (void)environment;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return value;
}

static LT_Value special_form_lambda(LT_Value arguments,
                                    LT_Environment* environment){
    LT_Value cursor = arguments;
    LT_Value parameters;
    LT_Value body;
    LT_Value parameter_cursor;

    LT_OBJECT_ARG(cursor, parameters);
    LT_ARG_REST(cursor, body);
    if (body == LT_NIL){
        LT_error("Special form lambda expects body");
    }

    parameter_cursor = parameters;
    if (LT_Symbol_p(parameter_cursor)){
        return LT_Closure_new(parameters, body, environment);
    }

    while (LT_Pair_p(parameter_cursor)){
        LT_Value parameter;
        parameter = LT_car(parameter_cursor);
        if (!LT_Symbol_p(parameter)){
            LT_error("Lambda parameter must be symbol");
        }
        parameter_cursor = LT_cdr(parameter_cursor);
    }
    if (parameter_cursor != LT_NIL && !LT_Symbol_p(parameter_cursor)){
        LT_error("Lambda parameters must be proper or dotted with symbol");
    }

    return LT_Closure_new(parameters, body, environment);
}

static LT_Value special_form_if(LT_Value arguments,
                                LT_Environment* environment){
    LT_Value cursor = arguments;
    LT_Value condition_expression;
    LT_Value then_expression;
    LT_Value else_expression = LT_NIL;
    LT_Value condition_value;

    LT_OBJECT_ARG(cursor, condition_expression);
    LT_OBJECT_ARG(cursor, then_expression);
    LT_OBJECT_ARG_OPT(cursor, else_expression, LT_NIL);
    LT_ARG_END(cursor);

    condition_value = LT_eval(condition_expression, environment);
    if (condition_value != LT_NIL){
        return LT_eval(then_expression, environment);
    }
    if (else_expression == LT_NIL){
        return LT_NIL;
    }
    return LT_eval(else_expression, environment);
}

static LT_Value special_form_define(LT_Value arguments,
                                    LT_Environment* environment){
    LT_Value cursor = arguments;
    LT_Value symbol;
    LT_Value value_expression;
    LT_Value value;

    LT_OBJECT_ARG(cursor, symbol);
    LT_OBJECT_ARG(cursor, value_expression);

    if (!LT_Symbol_p(symbol)){
        LT_error("Special form define expects symbol as first argument");
    }
    LT_ARG_END(cursor);

    value = LT_eval(value_expression, environment);
    LT_Environment_bind(environment, symbol, value, 0);
    return value;
}

static LT_Value special_form_set_bang(LT_Value arguments,
                                      LT_Environment* environment){
    LT_Value cursor = arguments;
    LT_Value symbol;
    LT_Value value_expression;
    LT_Value value;

    LT_OBJECT_ARG(cursor, symbol);
    LT_OBJECT_ARG(cursor, value_expression);

    if (!LT_Symbol_p(symbol)){
        LT_error("Special form set! expects symbol as first argument");
    }
    LT_ARG_END(cursor);

    value = LT_eval(value_expression, environment);
    if (!LT_Environment_set(environment, symbol, value)){
        LT_error("Special form set! expected existing mutable binding");
    }
    return value;
}

static int LT_Macro_p_implementation(LT_Value value){
    return LT_Primitive_p(value)
        || LT_Closure_p(value);
}

static LT_Value special_form_macro(LT_Value arguments,
                                   LT_Environment* environment){
    LT_Value cursor = arguments;
    LT_Value callable_expression;
    LT_Value callable;

    LT_OBJECT_ARG(cursor, callable_expression);
    LT_ARG_END(cursor);

    callable = LT_eval(callable_expression, environment);
    if (!LT_Macro_p_implementation(callable)){
        LT_error("Special form macro expects primitive or closure");
    }

    return LT_Macro_new(callable);
}

static LT_SpecialForm quote_special_form = {
    .function = special_form_quote,
    .name = "quote",
    .arguments = "(value)",
    .description = "Return value without evaluation."
};

static LT_SpecialForm lambda_special_form = {
    .function = special_form_lambda,
    .name = "lambda",
    .arguments = "((arg ...) body ...)",
    .description = "Create closure with lexical scope."
};

static LT_SpecialForm if_special_form = {
    .function = special_form_if,
    .name = "if",
    .arguments = "(condition then [else])",
    .description = "Evaluate then or else based on condition."
};

static LT_SpecialForm define_special_form = {
    .function = special_form_define,
    .name = "define",
    .arguments = "(symbol value-expression)",
    .description = "Create mutable binding in current environment."
};

static LT_SpecialForm set_bang_special_form = {
    .function = special_form_set_bang,
    .name = "set!",
    .arguments = "(symbol value-expression)",
    .description = "Update existing mutable binding."
};

static LT_SpecialForm macro_special_form = {
    .function = special_form_macro,
    .name = "macro",
    .arguments = "(callable-expression)",
    .description = "Wrap primitive or closure as macro."
};

static void bind_static_special_form(LT_Environment* environment,
                                     LT_SpecialForm* special_form){
    LT_Environment_bind(
        environment,
        LT_Symbol_new(special_form->name),
        LT_SpecialForm_from_static(special_form),
        LT_ENV_BINDING_FLAG_CONSTANT
    );
}

void LT_base_env_bind_special_forms(LT_Environment* environment){
    bind_static_special_form(environment, &quote_special_form);
    bind_static_special_form(environment, &lambda_special_form);
    bind_static_special_form(environment, &if_special_form);
    bind_static_special_form(environment, &define_special_form);
    bind_static_special_form(environment, &set_bang_special_form);
    bind_static_special_form(environment, &macro_special_form);
}
