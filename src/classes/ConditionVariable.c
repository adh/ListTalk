/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/ConditionVariable.h>
#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>

struct LT_ConditionVariable_s {
    LT_Object base;
    LT_CondWord cond;
    LT_Value name;
};

static void ConditionVariable_debugPrintOn(LT_Value obj, FILE* stream){
    LT_ConditionVariable* cond = LT_ConditionVariable_from_value(obj);
    if (cond->name != LT_NIL){
        fputs("#<ConditionVariable ", stream);
        LT_Value_debugPrintOn(cond->name, stream);
        fprintf(stream, " %p>", (void*)cond);
    } else {
        fprintf(stream, "#<ConditionVariable %p>", (void*)cond);
    }
}

LT_DEFINE_PRIMITIVE(
    condition_variable_class_method_new,
    "ConditionVariable class>>new",
    "(self)",
    "Return a new condition variable."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_ConditionVariable_class){
        LT_error("new class method is only supported on ConditionVariable");
    }
    return (LT_Value)(uintptr_t)LT_ConditionVariable_new();
}

LT_DEFINE_PRIMITIVE(
    condition_variable_class_method_new_named,
    "ConditionVariable class>>new:",
    "(self name)",
    "Return a new named condition variable."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value name;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, name);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_ConditionVariable_class){
        LT_error("new: class method is only supported on ConditionVariable");
    }
    return (LT_Value)(uintptr_t)LT_ConditionVariable_new_named(name);
}

LT_DEFINE_PRIMITIVE(
    condition_variable_method_wait,
    "ConditionVariable>>wait:",
    "(self mutex)",
    "Release mutex, wait for notification, reacquire mutex, and return receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value mutex;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, mutex);
    LT_ARG_END(cursor);
    LT_ConditionVariable_wait(
        LT_ConditionVariable_from_value(self),
        LT_Mutex_from_value(mutex)
    );
    return self;
}

LT_DEFINE_PRIMITIVE(
    condition_variable_method_signal,
    "ConditionVariable>>signal",
    "(self)",
    "Wake one thread waiting on condition variable and return receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    LT_ConditionVariable_signal(LT_ConditionVariable_from_value(self));
    return self;
}

LT_DEFINE_PRIMITIVE(
    condition_variable_method_broadcast,
    "ConditionVariable>>broadcast",
    "(self)",
    "Wake all threads waiting on condition variable and return receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    LT_ConditionVariable_broadcast(LT_ConditionVariable_from_value(self));
    return self;
}

LT_DEFINE_PRIMITIVE(
    condition_variable_method_name,
    "ConditionVariable>>name",
    "(self)",
    "Return condition variable name, or nil when it has none."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    return LT_ConditionVariable_name(LT_ConditionVariable_from_value(self));
}

static LT_Method_Descriptor ConditionVariable_methods[] = {
    {"wait:", &condition_variable_method_wait},
    {"signal", &condition_variable_method_signal},
    {"broadcast", &condition_variable_method_broadcast},
    {"name", &condition_variable_method_name},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor ConditionVariable_class_methods[] = {
    {"new", &condition_variable_class_method_new},
    {"new:", &condition_variable_class_method_new_named},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_ConditionVariable) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "ConditionVariable",
    .documentation = "Condition variable associated with a mutex while waiting.",
    .instance_size = sizeof(LT_ConditionVariable),
    .debugPrintOn = ConditionVariable_debugPrintOn,
    .methods = ConditionVariable_methods,
    .class_methods = ConditionVariable_class_methods,
};

LT_ConditionVariable* LT_ConditionVariable_new(void){
    return LT_ConditionVariable_new_named(LT_NIL);
}

LT_ConditionVariable* LT_ConditionVariable_new_named(LT_Value name){
    LT_ConditionVariable* cond = LT_Class_ALLOC(LT_ConditionVariable);

    LT_CondWord_init(&cond->cond);
    cond->name = name;
    return cond;
}

void LT_ConditionVariable_wait(LT_ConditionVariable* cond, LT_Mutex* mutex){
    LT_CondWord_wait(&cond->cond, LT_Mutex_lock_word(mutex));
}

void LT_ConditionVariable_signal(LT_ConditionVariable* cond){
    LT_CondWord_signal(&cond->cond);
}

void LT_ConditionVariable_broadcast(LT_ConditionVariable* cond){
    LT_CondWord_broadcast(&cond->cond);
}

LT_Value LT_ConditionVariable_name(LT_ConditionVariable* cond){
    return cond->name;
}
