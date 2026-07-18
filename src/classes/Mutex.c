/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Mutex.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>

struct LT_Mutex_s {
    LT_Object base;
    LT_MutexWord lock;
};

static void Mutex_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Mutex* mutex = LT_Mutex_from_value(obj);
    fprintf(stream, "#<Mutex %p>", (void*)mutex);
}

LT_DEFINE_PRIMITIVE(
    mutex_class_method_new,
    "Mutex class>>new",
    "(self)",
    "Return a new mutex."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_Mutex_class){
        LT_error("new class method is only supported on Mutex");
    }
    return (LT_Value)(uintptr_t)LT_Mutex_new();
}

LT_DEFINE_PRIMITIVE(
    mutex_method_lock,
    "Mutex>>lock",
    "(self)",
    "Lock mutex and return receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    LT_Mutex_lock(LT_Mutex_from_value(self));
    return self;
}

LT_DEFINE_PRIMITIVE(
    mutex_method_try_lock,
    "Mutex>>tryLock",
    "(self)",
    "Try to lock mutex and return whether it was acquired."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Mutex_tryLock(LT_Mutex_from_value(self)) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    mutex_method_unlock,
    "Mutex>>unlock",
    "(self)",
    "Unlock mutex and return receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    LT_Mutex_unlock(LT_Mutex_from_value(self));
    return self;
}

static LT_Method_Descriptor Mutex_methods[] = {
    {"lock", &mutex_method_lock},
    {"tryLock", &mutex_method_try_lock},
    {"unlock", &mutex_method_unlock},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor Mutex_class_methods[] = {
    {"new", &mutex_class_method_new},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Mutex) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Mutex",
    .documentation = "Mutual exclusion lock.",
    .instance_size = sizeof(LT_Mutex),
    .debugPrintOn = Mutex_debugPrintOn,
    .methods = Mutex_methods,
    .class_methods = Mutex_class_methods,
};

LT_Mutex* LT_Mutex_new(void){
    LT_Mutex* mutex = LT_Class_ALLOC(LT_Mutex);

    LT_MutexWord_init(&mutex->lock);
    return mutex;
}

void LT_Mutex_lock(LT_Mutex* mutex){
    LT_MutexWord_lock(&mutex->lock);
}

int LT_Mutex_tryLock(LT_Mutex* mutex){
    return LT_MutexWord_try_lock(&mutex->lock);
}

void LT_Mutex_unlock(LT_Mutex* mutex){
    LT_MutexWord_unlock(&mutex->lock);
}
