/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Function.h>
#include <ListTalk/classes/Future.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Promise.h>
#include <ListTalk/ListTalk.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/utils/lock.h>
#include <ListTalk/classes/Class.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/throw_catch.h>

#include <pthread.h>

struct LT_Promise_s {
    LT_Object base;
    LT_MutexWord lock;
    LT_CondWord cond;
    LT_Value thunk;
    LT_Value value;
    pthread_t resolving_thread;
    bool has_value : 1;
    bool resolving : 1;
    bool resolving_thread_valid : 1;
};

static void Promise_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Promise* promise = LT_Promise_from_value(obj);

    fprintf(
        stream,
        promise->has_value ? "#<Promise resolved %p>" : "#<Promise pending %p>",
        (void*)promise
    );
}

LT_DEFINE_PRIMITIVE(
    promise_class_method_delay,
    "Promise class>>delay:",
    "(self thunk)",
    "Create a promise that will call thunk when forced."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value thunk;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, thunk);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_Promise_class){
        LT_error("delay: class method is only supported on Promise");
    }
    return (LT_Value)(uintptr_t)LT_Promise_delay(thunk);
}

LT_DEFINE_PRIMITIVE(
    promise_method_force,
    "Promise>>force!",
    "(self)",
    "Force promise evaluation and return its value."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Promise_force(LT_Promise_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    promise_method_value,
    "Promise>>value",
    "(self)",
    "Return promise value, or raise error if promise has not resolved."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Promise_value(LT_Promise_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    promise_method_hasValue,
    "Promise>>hasValue?",
    "(self)",
    "Return true when the promise has a memoized value."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Promise_hasValue_p(LT_Promise_from_value(self)) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    promise_method_resolved,
    "Promise>>resolved?",
    "(self)",
    "Return true when the promise is resolved."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Promise_hasValue_p(LT_Promise_from_value(self)) ? LT_TRUE : LT_FALSE;
}

static LT_Method_Descriptor Promise_methods[] = {
    {"force!", &promise_method_force},
    {"value", &promise_method_value},
    {"hasValue?", &promise_method_hasValue},
    {"resolved?", &promise_method_resolved},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor Promise_class_methods[] = {
    {"delay:", &promise_class_method_delay},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Promise) {
    .superclass = &LT_Future_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Promise",
    .documentation = "Delayed computation evaluated at most once when forced.",
    .instance_size = sizeof(LT_Promise),
    .debugPrintOn = Promise_debugPrintOn,
    .methods = Promise_methods,
    .class_methods = Promise_class_methods,
};

LT_Promise* LT_Promise_delay(LT_Value thunk){
    LT_Promise* promise;

    if (!LT_Value_is_instance_of(thunk, LT_STATIC_CLASS(LT_Function))){
        LT_type_error(thunk, &LT_Function_class);
    }

    promise = LT_Class_ALLOC(LT_Promise);
    LT_MutexWord_init(&promise->lock);
    LT_CondWord_init(&promise->cond);
    promise->thunk = thunk;
    promise->value = LT_NIL;
    promise->has_value = 0;
    promise->resolving = 0;
    promise->resolving_thread_valid = 0;
    return promise;
}

LT_Value LT_Promise_force(LT_Promise* promise){
    LT_Value thunk;
    LT_Value result = LT_NIL;
    bool succeeded = false;

    LT_MutexWord_lock(&promise->lock);
    while (promise->resolving){
        if (promise->resolving_thread_valid
            && pthread_equal(promise->resolving_thread, pthread_self())){
            LT_MutexWord_unlock(&promise->lock);
            LT_error("Promise is already being forced by this thread");
        }
        LT_CondWord_wait(&promise->cond, &promise->lock);
    }
    if (promise->has_value){
        result = promise->value;
        LT_MutexWord_unlock(&promise->lock);
        return result;
    }
    promise->resolving = 1;
    promise->resolving_thread = pthread_self();
    promise->resolving_thread_valid = 1;
    thunk = promise->thunk;
    LT_MutexWord_unlock(&promise->lock);

    LT_UNWIND_PROTECT({
        result = LT_apply(thunk, LT_NIL, LT_NIL, LT_NIL, NULL);
        succeeded = true;
    }, {
        LT_MutexWord_lock(&promise->lock);
        if (succeeded){
            promise->value = result;
            promise->thunk = LT_NIL;
            promise->has_value = 1;
        }
        promise->resolving = 0;
        promise->resolving_thread_valid = 0;
        LT_CondWord_broadcast(&promise->cond);
        LT_MutexWord_unlock(&promise->lock);
    });

    return result;
}

LT_Value LT_Promise_value(LT_Promise* promise){
    LT_Value value;

    LT_MutexWord_lock(&promise->lock);
    if (!promise->has_value){
        LT_MutexWord_unlock(&promise->lock);
        LT_error("Promise has not resolved");
    }
    value = promise->value;
    LT_MutexWord_unlock(&promise->lock);
    return value;
}

bool LT_Promise_hasValue_p(LT_Promise* promise){
    bool has_value;

    LT_MutexWord_lock(&promise->lock);
    has_value = promise->has_value;
    LT_MutexWord_unlock(&promise->lock);
    return has_value;
}
