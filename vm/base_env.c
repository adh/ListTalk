/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/vm/base_env.h>

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Closure.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/SpecialForm.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/vm/error.h>

static LT_Value list_pop_arg(LT_Value* cursor, char* primitive_name){
    LT_Value arg;

    if (*cursor == LT_VALUE_NIL){
        LT_error("Not enough arguments to primitive");
    }

    if (!LT_Value_is_pair(*cursor)){
        LT_error("Primitive expected a proper list of arguments");
    }

    arg = LT_car(*cursor);
    *cursor = LT_cdr(*cursor);
    (void)primitive_name;
    return arg;
}

static LT_Value fixnum_arg(LT_Value value, char* primitive_name){
    if (!LT_Value_is_fixnum(value)){
        (void)primitive_name;
        LT_error("Primitive expected fixnum argument");
    }
    return value;
}

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

    while (cursor != LT_VALUE_NIL){
        LT_Value arg = fixnum_arg(list_pop_arg(&cursor, "+"), "+");
        sum += LT_Value_fixnum_value(arg);
    }

    return checked_fixnum_from_i128(sum, "+");
}

static LT_Value primitive_subtract(LT_Value arguments){
    LT_Value cursor = arguments;
    LT_Value first = fixnum_arg(list_pop_arg(&cursor, "-"), "-");
    __int128 result = LT_Value_fixnum_value(first);

    if (cursor == LT_VALUE_NIL){
        result = -result;
        return checked_fixnum_from_i128(result, "-");
    }

    while (cursor != LT_VALUE_NIL){
        LT_Value arg = fixnum_arg(list_pop_arg(&cursor, "-"), "-");
        result -= LT_Value_fixnum_value(arg);
    }

    return checked_fixnum_from_i128(result, "-");
}

static LT_Value primitive_multiply(LT_Value arguments){
    __int128 product = 1;
    LT_Value cursor = arguments;

    while (cursor != LT_VALUE_NIL){
        LT_Value arg = fixnum_arg(list_pop_arg(&cursor, "*"), "*");
        product *= LT_Value_fixnum_value(arg);
        if (product < LT_VALUE_FIXNUM_MIN || product > LT_VALUE_FIXNUM_MAX){
            LT_error("Fixnum arithmetic overflow");
        }
    }

    return checked_fixnum_from_i128(product, "*");
}

static LT_Value primitive_divide(LT_Value arguments){
    LT_Value cursor = arguments;
    LT_Value first = fixnum_arg(list_pop_arg(&cursor, "/"), "/");
    __int128 result = LT_Value_fixnum_value(first);

    if (cursor == LT_VALUE_NIL){
        LT_error("Primitive / expects at least two arguments");
    }

    while (cursor != LT_VALUE_NIL){
        LT_Value arg = fixnum_arg(list_pop_arg(&cursor, "/"), "/");
        int64_t divisor = LT_Value_fixnum_value(arg);
        if (divisor == 0){
            LT_error("Division by zero");
        }
        result /= divisor;
    }

    return checked_fixnum_from_i128(result, "/");
}

static LT_Value special_form_quote(LT_Value arguments,
                                   LT_Environment* environment){
    LT_Value cursor = arguments;
    LT_Value value;
    (void)environment;

    value = list_pop_arg(&cursor, "quote");
    if (cursor != LT_VALUE_NIL){
        LT_error("Special form quote expects one argument");
    }
    return value;
}

static LT_Value special_form_lambda(LT_Value arguments,
                                    LT_Environment* environment){
    LT_Value cursor = arguments;
    LT_Value parameters;
    LT_Value body;
    LT_Value parameter_cursor;

    parameters = list_pop_arg(&cursor, "lambda");
    body = cursor;
    if (body == LT_VALUE_NIL){
        LT_error("Special form lambda expects body");
    }

    parameter_cursor = parameters;
    while (parameter_cursor != LT_VALUE_NIL){
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
    LT_Value else_expression = LT_VALUE_NIL;
    LT_Value condition_value;

    condition_expression = list_pop_arg(&cursor, "if");
    then_expression = list_pop_arg(&cursor, "if");

    if (cursor != LT_VALUE_NIL){
        else_expression = list_pop_arg(&cursor, "if");
    }
    if (cursor != LT_VALUE_NIL){
        LT_error("Special form if expects two or three arguments");
    }

    condition_value = LT_eval(condition_expression, environment);
    if (condition_value != LT_VALUE_NIL){
        return LT_eval(then_expression, environment);
    }
    if (else_expression == LT_VALUE_NIL){
        return LT_VALUE_NIL;
    }
    return LT_eval(else_expression, environment);
}

static LT_Value special_form_define(LT_Value arguments,
                                    LT_Environment* environment){
    LT_Value cursor = arguments;
    LT_Value symbol = list_pop_arg(&cursor, "define");
    LT_Value value_expression = list_pop_arg(&cursor, "define");
    LT_Value value;

    if (!LT_Value_is_symbol(symbol)){
        LT_error("Special form define expects symbol as first argument");
    }
    if (cursor != LT_VALUE_NIL){
        LT_error("Special form define expects two arguments");
    }

    value = LT_eval(value_expression, environment);
    LT_Environment_bind(environment, symbol, value, 0);
    return value;
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
    bind_special_form(environment, "quote", special_form_quote);
    bind_special_form(environment, "lambda", special_form_lambda);
    bind_special_form(environment, "if", special_form_if);
    bind_special_form(environment, "define", special_form_define);
    return environment;
}

LT_Environment* LT_get_shared_base_environment(void){
    static LT_Environment* shared = NULL;

    if (shared == NULL){
        shared = LT_new_base_environment();
    }

    return shared;
}
