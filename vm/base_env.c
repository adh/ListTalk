/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/vm/base_env.h>

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Closure.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Macro.h>
#include <ListTalk/classes/SpecialForm.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/error.h>

static LT_Value checked_fixnum_from_i128(__int128 result, char* primitive_name){
    (void)primitive_name;
    if (result < LT_VALUE_FIXNUM_MIN || result > LT_VALUE_FIXNUM_MAX){
        LT_error("Fixnum arithmetic overflow");
    }

    return LT_Value_fixnum((int64_t)result);
}

static LT_Value primitive_add(LT_Value arguments){
    __int128 sum = 0;
    LT_Value cursor = arguments;

    while (cursor != LT_NIL){
        int64_t arg_value;
        LT_FIXNUM_ARG(cursor, arg_value);
        sum += arg_value;
    }

    return checked_fixnum_from_i128(sum, "+");
}

static LT_Value primitive_subtract(LT_Value arguments){
    LT_Value cursor = arguments;
    int64_t first_value;
    __int128 result;

    LT_FIXNUM_ARG(cursor, first_value);
    result = first_value;

    if (cursor == LT_NIL){
        result = -result;
        return checked_fixnum_from_i128(result, "-");
    }

    while (cursor != LT_NIL){
        int64_t arg_value;
        LT_FIXNUM_ARG(cursor, arg_value);
        result -= arg_value;
    }

    return checked_fixnum_from_i128(result, "-");
}

static LT_Value primitive_multiply(LT_Value arguments){
    __int128 product = 1;
    LT_Value cursor = arguments;

    while (cursor != LT_NIL){
        int64_t arg_value;
        LT_FIXNUM_ARG(cursor, arg_value);
        product *= arg_value;
        if (product < LT_VALUE_FIXNUM_MIN || product > LT_VALUE_FIXNUM_MAX){
            LT_error("Fixnum arithmetic overflow");
        }
    }

    return checked_fixnum_from_i128(product, "*");
}

static LT_Value primitive_divide(LT_Value arguments){
    LT_Value cursor = arguments;
    int64_t first_value;
    __int128 result;

    LT_FIXNUM_ARG(cursor, first_value);
    result = first_value;

    if (cursor == LT_NIL){
        LT_error("Primitive / expects at least two arguments");
    }

    while (cursor != LT_NIL){
        int64_t divisor;
        LT_FIXNUM_ARG(cursor, divisor);
        if (divisor == 0){
            LT_error("Division by zero");
        }
        result /= divisor;
    }

    return checked_fixnum_from_i128(result, "/");
}

static LT_Value primitive_type_of(LT_Value arguments){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_Value_class(value);
}

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
    while (parameter_cursor != LT_NIL){
        LT_Value parameter;
        if (!LT_Value_is_pair(parameter_cursor)){
            LT_error("Lambda parameters must be proper list");
        }
        parameter = LT_car(parameter_cursor);
        if (!LT_Value_is_symbol(parameter)){
            LT_error("Lambda parameter must be symbol");
        }
        parameter_cursor = LT_cdr(parameter_cursor);
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

    if (!LT_Value_is_symbol(symbol)){
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

    if (!LT_Value_is_symbol(symbol)){
        LT_error("Special form set! expects symbol as first argument");
    }
    LT_ARG_END(cursor);

    value = LT_eval(value_expression, environment);
    if (!LT_Environment_set(environment, symbol, value)){
        LT_error("Special form set! expected existing mutable binding");
    }
    return value;
}

static int LT_Value_is_macro_implementation(LT_Value value){
    return LT_Value_is_primitive(value)
        || LT_Value_is_closure(value);
}

static LT_Value special_form_macro(LT_Value arguments,
                                   LT_Environment* environment){
    LT_Value cursor = arguments;
    LT_Value callable_expression;
    LT_Value callable;

    LT_OBJECT_ARG(cursor, callable_expression);
    LT_ARG_END(cursor);

    callable = LT_eval(callable_expression, environment);
    if (!LT_Value_is_macro_implementation(callable)){
        LT_error("Special form macro expects primitive or closure");
    }

    return LT_Macro_new(callable);
}

static void bind_primitive(LT_Environment* environment,
                           char* name,
                           LT_Primitive_Func function){
    LT_Environment_bind(
        environment,
        LT_Symbol_new(name),
        LT_Primitive_new(name, function),
        LT_ENV_BINDING_FLAG_CONSTANT
    );
}

static void bind_special_form(LT_Environment* environment,
                              char* name,
                              LT_SpecialForm_Func function){
    LT_Environment_bind(
        environment,
        LT_Symbol_new(name),
        LT_SpecialForm_new(name, function),
        LT_ENV_BINDING_FLAG_CONSTANT
    );
}

LT_Environment* LT_new_base_environment(void){
    LT_Environment* environment = LT_Environment_new(NULL);
    bind_primitive(environment, "+", primitive_add);
    bind_primitive(environment, "-", primitive_subtract);
    bind_primitive(environment, "*", primitive_multiply);
    bind_primitive(environment, "/", primitive_divide);
    bind_primitive(environment, "type-of", primitive_type_of);
    bind_special_form(environment, "quote", special_form_quote);
    bind_special_form(environment, "lambda", special_form_lambda);
    bind_special_form(environment, "if", special_form_if);
    bind_special_form(environment, "define", special_form_define);
    bind_special_form(environment, "set!", special_form_set_bang);
    bind_special_form(environment, "macro", special_form_macro);
    return environment;
}

LT_Environment* LT_get_shared_base_environment(void){
    static LT_Environment* shared = NULL;

    if (shared == NULL){
        shared = LT_new_base_environment();
    }

    return shared;
}
