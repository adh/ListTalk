/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/vm/base_env.h>

#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Primitive.h>
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

LT_Environment* LT_new_base_environment(void){
    LT_Environment* environment = LT_Environment_new(NULL);
    bind_primitive(environment, "+", primitive_add);
    bind_primitive(environment, "-", primitive_subtract);
    bind_primitive(environment, "*", primitive_multiply);
    bind_primitive(environment, "/", primitive_divide);
    return environment;
}

LT_Environment* LT_get_shared_base_environment(void){
    static LT_Environment* shared = NULL;

    if (shared == NULL){
        shared = LT_new_base_environment();
    }

    return shared;
}
