/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/classes/String.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/compiler.h>
#include <ListTalk/vm/error.h>

LT_DEFINE_PRIMITIVE(
    primitive_type_of,
    "type-of",
    "(value)",
    "Return class descriptor of value."
){
    LT_Value cursor = arguments;
    LT_Value value;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_Value_class(value);
}

LT_DEFINE_PRIMITIVE(
    primitive_error,
    "error",
    "(message)",
    "Signal error condition and abort unless intercepted."
){
    LT_Value cursor = arguments;
    LT_Value message;

    LT_OBJECT_ARG(cursor, message);
    LT_ARG_END(cursor);

    if (!LT_String_p(message)){
        LT_type_error(message, &LT_String_class);
    }

    LT_error(LT_String_value_cstr(LT_String_from_value(message)));
    return LT_NIL;
}

LT_DEFINE_PRIMITIVE(
    primitive_macroexpand,
    "macroexpand",
    "(form environment)",
    "Expand macros in form using environment object."
){
    LT_Value cursor = arguments;
    LT_Value expression;
    LT_Value environment_value;
    LT_Environment* lexical_environment;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, expression);
    LT_OBJECT_ARG(cursor, environment_value);
    LT_ARG_END(cursor);

    lexical_environment = LT_Environment_from_value(environment_value);
    return LT_compiler_macroexpand(expression, lexical_environment);
}

LT_DEFINE_PRIMITIVE(
    primitive_fold_expression,
    "fold-expression",
    "(form environment)",
    "Fold form with lexical constants from environment object."
){
    LT_Value cursor = arguments;
    LT_Value expression;
    LT_Value environment_value;
    LT_Environment* lexical_environment;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, expression);
    LT_OBJECT_ARG(cursor, environment_value);
    LT_ARG_END(cursor);

    lexical_environment = LT_Environment_from_value(environment_value);
    return LT_compiler_fold_expression(expression, lexical_environment);
}

void LT_base_env_bind_primitives(LT_Environment* environment){
    LT_base_env_bind_static_primitive(environment, &primitive_type_of);
    LT_base_env_bind_static_primitive(environment, &primitive_error);
    LT_base_env_bind_static_primitive(environment, &primitive_macroexpand);
    LT_base_env_bind_static_primitive(environment, &primitive_fold_expression);
}
