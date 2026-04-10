/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Package.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/vm/loader.h>

#include <stdlib.h>

LT_DEFINE_PRIMITIVE(
    primitive_os_exit,
    "exit",
    "([status])",
    "Terminate process with optional fixnum status code."
){
    LT_Value cursor = arguments;
    LT_Value status = LT_SmallInteger_new(0);
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_OBJECT_ARG_OPT(cursor, status, status);
    LT_ARG_END(cursor);

    if (!LT_Value_is_fixnum(status)){
        LT_type_error(status, &LT_SmallInteger_class);
    }

    exit((int)LT_SmallInteger_value(status));
    return LT_NIL;
}

void ListTalk_os_load(LT_Environment* environment){
    LT_Package* package = LT_Package_new("ListTalk-OS");

    LT_Environment_bind(
        environment,
        LT_Symbol_new_in(package, primitive_os_exit.name),
        LT_Primitive_from_static(&primitive_os_exit),
        LT_ENV_BINDING_FLAG_CONSTANT
    );
    LT_loader_provide(environment, "os");
}
