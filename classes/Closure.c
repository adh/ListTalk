/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Closure.h>
#include <ListTalk/vm/Class.h>

struct LT_Closure_s {
    LT_Value parameters;
    LT_Value body;
    LT_Environment* environment;
};

LT_DEFINE_CLASS(LT_Closure) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Closure",
    .instance_size = sizeof(LT_Closure),
    .class_flags = LT_CLASS_FLAG_SPECIAL,
};

LT_Value LT_Closure_new(LT_Value parameters,
                        LT_Value body,
                        LT_Environment* environment){
    LT_Closure* closure = GC_NEW(LT_Closure);
    closure->parameters = parameters;
    closure->body = body;
    closure->environment = environment;
    return ((LT_Value)(uintptr_t)closure) | LT_VALUE_POINTER_TAG_CLOSURE;
}

LT_Value LT_Closure_parameters(LT_Closure* closure){
    return closure->parameters;
}

LT_Value LT_Closure_body(LT_Closure* closure){
    return closure->body;
}

LT_Environment* LT_Closure_environment(LT_Closure* closure){
    return closure->environment;
}
