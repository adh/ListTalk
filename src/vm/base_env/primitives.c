/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include "internal.h"

#include <ListTalk/classes/Printer.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Object.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/compiler.h>
#include <ListTalk/vm/error.h>

#include <stdio.h>
#include <stdlib.h>

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
    primitive_slot_ref,
    "slot-ref",
    "(object slot)",
    "Read object slot value by symbol name."
){
    LT_Value cursor = arguments;
    LT_Value object;
    LT_Value slot_name;

    LT_OBJECT_ARG(cursor, object);
    LT_OBJECT_ARG(cursor, slot_name);
    LT_ARG_END(cursor);

    return LT_Object_slot_ref(object, slot_name);
}

LT_DEFINE_PRIMITIVE(
    primitive_slot_set,
    "slot-set!",
    "(object slot value)",
    "Set object slot value by symbol name."
){
    LT_Value cursor = arguments;
    LT_Value object;
    LT_Value slot_name;
    LT_Value value;

    LT_OBJECT_ARG(cursor, object);
    LT_OBJECT_ARG(cursor, slot_name);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);

    return LT_Object_slot_set(object, slot_name, value);
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
    primitive_display,
    "display",
    "(value)",
    "Print value and newline to standard output, then return it."
){
    LT_Value cursor = arguments;
    LT_Value value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);

    if (LT_String_p(value)){
        fputs(LT_String_value_cstr(LT_String_from_value(value)), stdout);
    } else {
        LT_printer_print_object(value);
    }
    fputc('\n', stdout);
    fflush(stdout);
    return value;
}

LT_DEFINE_PRIMITIVE(
    primitive_read,
    "read",
    "()",
    "Read one line from standard input and return it as a string."
){
    LT_Value cursor = arguments;
    LT_StringBuilder* builder;
    int ch;
    (void)tail_call_unwind_marker;

    LT_ARG_END(cursor);

    builder = LT_StringBuilder_new();
    ch = fgetc(stdin);
    if (ch == EOF){
        return LT_NIL;
    }

    while (ch != EOF && ch != '\n'){
        if (ch != '\r'){
            LT_StringBuilder_append_char(builder, (char)ch);
        }
        ch = fgetc(stdin);
    }

    return (LT_Value)(uintptr_t)LT_String_new(
        LT_StringBuilder_value(builder),
        LT_StringBuilder_length(builder)
    );
}

LT_DEFINE_PRIMITIVE(
    primitive_exit,
    "exit",
    "([status])",
    "Terminate process with optional fixnum status code."
){
    LT_Value cursor = arguments;
    LT_Value status = LT_SmallInteger_new(0);
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG_OPT(cursor, status, status);
    LT_ARG_END(cursor);

    if (!LT_Value_is_fixnum(status)){
        LT_type_error(status, &LT_SmallInteger_class);
    }

    exit((int)LT_SmallInteger_value(status));
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
    LT_base_env_bind_static_primitive(environment, &primitive_slot_ref);
    LT_base_env_bind_static_primitive(environment, &primitive_slot_set);
    LT_base_env_bind_static_primitive(environment, &primitive_error);
    LT_base_env_bind_static_primitive(environment, &primitive_display);
    LT_base_env_bind_static_primitive(environment, &primitive_read);
    LT_base_env_bind_static_primitive(environment, &primitive_exit);
    LT_base_env_bind_static_primitive(environment, &primitive_macroexpand);
    LT_base_env_bind_static_primitive(environment, &primitive_fold_expression);
}
