/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/ReadWriteLock.h>
#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/throw_catch.h>

struct LT_ReadWriteLock_s {
    LT_Object base;
    LT_RWLockWord lock;
    LT_Value name;
};

static void ReadWriteLock_debugPrintOn(LT_Value obj, FILE* stream){
    LT_ReadWriteLock* lock = LT_ReadWriteLock_from_value(obj);
    if (lock->name != LT_NIL){
        fputs("#<ReadWriteLock ", stream);
        LT_Value_debugPrintOn(lock->name, stream);
        fprintf(stream, " %p>", (void*)lock);
    } else {
        fprintf(stream, "#<ReadWriteLock %p>", (void*)lock);
    }
}

LT_DEFINE_PRIMITIVE(
    read_write_lock_class_method_new,
    "ReadWriteLock class>>new",
    "(self)",
    "Return a new read-write lock."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_ReadWriteLock_class){
        LT_error("new class method is only supported on ReadWriteLock");
    }
    return (LT_Value)(uintptr_t)LT_ReadWriteLock_new();
}

LT_DEFINE_PRIMITIVE(
    read_write_lock_class_method_new_named,
    "ReadWriteLock class>>new:",
    "(self name)",
    "Return a new named read-write lock."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value name;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, name);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_ReadWriteLock_class){
        LT_error("new: class method is only supported on ReadWriteLock");
    }
    return (LT_Value)(uintptr_t)LT_ReadWriteLock_new_named(name);
}

LT_DEFINE_PRIMITIVE(
    read_write_lock_method_read_lock,
    "ReadWriteLock>>readLock",
    "(self)",
    "Acquire read access and return receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    LT_ReadWriteLock_readLock(LT_ReadWriteLock_from_value(self));
    return self;
}

LT_DEFINE_PRIMITIVE(
    read_write_lock_method_try_read_lock,
    "ReadWriteLock>>tryReadLock",
    "(self)",
    "Try to acquire read access and return whether it was acquired."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_ReadWriteLock_tryReadLock(LT_ReadWriteLock_from_value(self))
        ? LT_TRUE
        : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    read_write_lock_method_read_unlock,
    "ReadWriteLock>>readUnlock",
    "(self)",
    "Release read access and return receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    LT_ReadWriteLock_readUnlock(LT_ReadWriteLock_from_value(self));
    return self;
}

LT_DEFINE_PRIMITIVE(
    read_write_lock_method_write_lock,
    "ReadWriteLock>>writeLock",
    "(self)",
    "Acquire write access and return receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    LT_ReadWriteLock_writeLock(LT_ReadWriteLock_from_value(self));
    return self;
}

LT_DEFINE_PRIMITIVE(
    read_write_lock_method_try_write_lock,
    "ReadWriteLock>>tryWriteLock",
    "(self)",
    "Try to acquire write access and return whether it was acquired."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_ReadWriteLock_tryWriteLock(LT_ReadWriteLock_from_value(self))
        ? LT_TRUE
        : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    read_write_lock_method_write_unlock,
    "ReadWriteLock>>writeUnlock",
    "(self)",
    "Release write access and return receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    LT_ReadWriteLock_writeUnlock(LT_ReadWriteLock_from_value(self));
    return self;
}

LT_DEFINE_PRIMITIVE(
    read_write_lock_method_read_do,
    "ReadWriteLock>>readDo:",
    "(self callable)",
    "Acquire read access, call callable, release read access, and return callable result."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    LT_Value result;
    LT_ReadWriteLock* lock;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);

    lock = LT_ReadWriteLock_from_value(self);
    LT_ReadWriteLock_readLock(lock);
    LT_UNWIND_PROTECT(
        {
            result = LT_apply(callable, LT_NIL, LT_NIL, LT_NIL, NULL);
        }, {
            LT_ReadWriteLock_readUnlock(lock);
        }
    );
    return result;
}

LT_DEFINE_PRIMITIVE(
    read_write_lock_method_write_do,
    "ReadWriteLock>>writeDo:",
    "(self callable)",
    "Acquire write access, call callable, release write access, and return callable result."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    LT_Value result;
    LT_ReadWriteLock* lock;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);

    lock = LT_ReadWriteLock_from_value(self);
    LT_ReadWriteLock_writeLock(lock);
    LT_UNWIND_PROTECT(
        {
            result = LT_apply(callable, LT_NIL, LT_NIL, LT_NIL, NULL);
        }, {
            LT_ReadWriteLock_writeUnlock(lock);
        }
    );
    return result;
}

LT_DEFINE_PRIMITIVE(
    read_write_lock_method_name,
    "ReadWriteLock>>name",
    "(self)",
    "Return read-write lock name, or nil when it has none."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    return LT_ReadWriteLock_name(LT_ReadWriteLock_from_value(self));
}

static LT_Method_Descriptor ReadWriteLock_methods[] = {
    {"readLock", &read_write_lock_method_read_lock},
    {"tryReadLock", &read_write_lock_method_try_read_lock},
    {"readUnlock", &read_write_lock_method_read_unlock},
    {"writeLock", &read_write_lock_method_write_lock},
    {"tryWriteLock", &read_write_lock_method_try_write_lock},
    {"writeUnlock", &read_write_lock_method_write_unlock},
    {"readDo:", &read_write_lock_method_read_do},
    {"writeDo:", &read_write_lock_method_write_do},
    {"name", &read_write_lock_method_name},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor ReadWriteLock_class_methods[] = {
    {"new", &read_write_lock_class_method_new},
    {"new:", &read_write_lock_class_method_new_named},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_ReadWriteLock) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "ReadWriteLock",
    .documentation = "Lock permitting multiple readers or one writer.",
    .instance_size = sizeof(LT_ReadWriteLock),
    .debugPrintOn = ReadWriteLock_debugPrintOn,
    .methods = ReadWriteLock_methods,
    .class_methods = ReadWriteLock_class_methods,
};

LT_ReadWriteLock* LT_ReadWriteLock_new(void){
    return LT_ReadWriteLock_new_named(LT_NIL);
}

LT_ReadWriteLock* LT_ReadWriteLock_new_named(LT_Value name){
    LT_ReadWriteLock* lock = LT_Class_ALLOC(LT_ReadWriteLock);

    LT_RWLockWord_init(&lock->lock);
    lock->name = name;
    return lock;
}

void LT_ReadWriteLock_readLock(LT_ReadWriteLock* lock){
    LT_RWLockWord_read_lock(&lock->lock);
}

int LT_ReadWriteLock_tryReadLock(LT_ReadWriteLock* lock){
    return LT_RWLockWord_try_read_lock(&lock->lock);
}

void LT_ReadWriteLock_readUnlock(LT_ReadWriteLock* lock){
    LT_RWLockWord_read_unlock(&lock->lock);
}

void LT_ReadWriteLock_writeLock(LT_ReadWriteLock* lock){
    LT_RWLockWord_write_lock(&lock->lock);
}

int LT_ReadWriteLock_tryWriteLock(LT_ReadWriteLock* lock){
    return LT_RWLockWord_try_write_lock(&lock->lock);
}

void LT_ReadWriteLock_writeUnlock(LT_ReadWriteLock* lock){
    LT_RWLockWord_write_unlock(&lock->lock);
}

LT_Value LT_ReadWriteLock_name(LT_ReadWriteLock* lock){
    return lock->name;
}
