/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Thread.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/ListTalk.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>

#include <errno.h>
#include <assert.h>

struct LT_Thread_s {
    LT_Object base;
    pthread_t pthread;
    LT_MutexWord state_lock;
    LT_Value callable;
    LT_Value result;
    bool finished : 1;
    bool joined : 1;
    bool joining : 1;
    bool detached : 1;
    bool managed : 1;
};

static void Thread_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Thread* thread = LT_Thread_from_value(obj);
    fprintf(stream, "#<Thread %p>", (void*)thread);
}

LT_DEFINE_PRIMITIVE(
    thread_class_method_new,
    "Thread class>>new:",
    "(self callable)",
    "Start callable in a new thread and return the thread."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_Thread_class){
        LT_error("new: class method is only supported on Thread");
    }
    return (LT_Value)(uintptr_t)LT_Thread_new(callable);
}

LT_DEFINE_PRIMITIVE(
    thread_class_method_current,
    "Thread class>>current",
    "(self)",
    "Return the current thread."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_Thread_class){
        LT_error("current class method is only supported on Thread");
    }
    return (LT_Value)(uintptr_t)LT_Thread_current();
}

LT_DEFINE_PRIMITIVE(
    thread_method_join,
    "Thread>>join",
    "(self)",
    "Wait for thread completion and return its result."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Thread_join(LT_Thread_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    thread_method_make_detached,
    "Thread>>makeDetached",
    "(self)",
    "Detach thread so it does not need to be joined."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    LT_Thread_makeDetached(LT_Thread_from_value(self));
    return self;
}

LT_DEFINE_PRIMITIVE(
    thread_method_detached_p,
    "Thread>>detached?",
    "(self)",
    "Return true when thread is detached."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Thread_detached_p(LT_Thread_from_value(self)) ? LT_TRUE : LT_FALSE;
}
LT_DEFINE_PRIMITIVE(
    thread_method_finished_p,
    "Thread>>finished?",
    "(self)",
    "Return true when thread has finished execution."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Thread_finished_p(LT_Thread_from_value(self)) ? LT_TRUE : LT_FALSE;
}
LT_DEFINE_PRIMITIVE(
    thread_method_joined_p,
    "Thread>>joined?",
    "(self)",
    "Return true when thread has been joined."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Thread_joined_p(LT_Thread_from_value(self)) ? LT_TRUE : LT_FALSE;
}
LT_DEFINE_PRIMITIVE(
    thread_method_result,
    "Thread>>result",
    "(self)",
    "Return thread result, or raise error if thread has not finished."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Thread_result(LT_Thread_from_value(self));
}   

static LT_Method_Descriptor Thread_methods[] = {
    {"join", &thread_method_join},
    {"makeDetached", &thread_method_make_detached},
    {"makeDaemon", &thread_method_make_detached},
    {"detached?", &thread_method_detached_p},
    {"finished?", &thread_method_finished_p},
    {"joined?", &thread_method_joined_p},
    {"result", &thread_method_result},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor Thread_class_methods[] = {
    {"new:", &thread_class_method_new},
    {"current", &thread_class_method_current},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Thread) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Thread",
    .documentation = "Thread executing a callable.",
    .instance_size = sizeof(LT_Thread),
    .debugPrintOn = Thread_debugPrintOn,
    .methods = Thread_methods,
    .class_methods = Thread_class_methods,
};

_Thread_local LT_Thread* current_thread = NULL;


static void* thread_main(void* opaque){
    LT_Thread* thread = opaque;
    LT_Value result;

    current_thread = thread;
    result = LT_apply(thread->callable, LT_NIL, LT_NIL, LT_NIL, NULL);

    LT_MutexWord_lock(&thread->state_lock);
    thread->result = result;
    thread->finished = 1;
    LT_MutexWord_unlock(&thread->state_lock);

    return NULL;
}

LT_Thread* LT_Thread_new(LT_Value callable){
    LT_Thread* thread = LT_Class_ALLOC(LT_Thread);
    int errnum;

    LT_MutexWord_init(&thread->state_lock);
    thread->callable = callable;
    thread->result = LT_NIL;
    thread->finished = 0;
    thread->joined = 0;
    thread->joining = 0;
    thread->detached = 0;
    thread->managed = 1;

    errnum = pthread_create(&thread->pthread, NULL, thread_main, thread);
    if (errnum != 0){
        LT_system_error("Could not create thread", errnum);
    }

    return thread;
}


LT_Thread* LT_Thread_current(void){
    if (current_thread == NULL){
        current_thread = LT_Class_ALLOC(LT_Thread);
        current_thread->pthread = pthread_self();
        LT_MutexWord_init(&current_thread->state_lock);
        current_thread->callable = LT_NIL;
        current_thread->result = LT_NIL;
        current_thread->finished = 1;
        current_thread->joined = 1;
        current_thread->detached = 0;
        current_thread->managed = 0;
    }
    return current_thread;
}
    

LT_Value LT_Thread_join(LT_Thread* thread){
    LT_Value result;
    int errnum;

    LT_MutexWord_lock(&thread->state_lock);
    if (thread->detached){
        LT_MutexWord_unlock(&thread->state_lock);
        LT_error("Cannot join daemon thread");
    }
    if (thread->managed == 0){
        LT_MutexWord_unlock(&thread->state_lock);
        LT_error("Cannot join unmanaged thread");
    }
    if (thread->joined){
        LT_MutexWord_unlock(&thread->state_lock);
        LT_error("Cannot join thread that has already been joined");
    }
    if (thread->joining){
        LT_MutexWord_unlock(&thread->state_lock);
        LT_error("Cannot join thread that is already being joined");
    }
    thread->joining = 1;
    LT_MutexWord_unlock(&thread->state_lock);

    errnum = pthread_join(thread->pthread, NULL);
    if (errnum != 0){
        LT_system_error("Could not join thread", errnum);
    }

    LT_MutexWord_lock(&thread->state_lock);
    assert(thread->finished);
    thread->joined = 1;
    thread->joining = 0;
    result = thread->result;

    LT_MutexWord_unlock(&thread->state_lock);
    return result;
}

void LT_Thread_makeDetached(LT_Thread* thread){
    int errnum;

    LT_MutexWord_lock(&thread->state_lock);
    if (thread->joined){
        LT_MutexWord_unlock(&thread->state_lock);
        LT_error("Cannot make joined thread daemon");
    }
    if (thread->managed == 0){
        LT_MutexWord_unlock(&thread->state_lock);
        LT_error("Cannot make unmanaged thread daemon");
    }
    if (thread->detached){
        LT_MutexWord_unlock(&thread->state_lock);
        return;
    }
    thread->detached = 1;
    LT_MutexWord_unlock(&thread->state_lock);

    errnum = pthread_detach(thread->pthread);
    if (errnum != 0){
        LT_system_error("Could not detach thread", errnum);
    }
}

bool LT_Thread_finished_p(LT_Thread* thread){
    int finished;

    LT_MutexWord_lock(&thread->state_lock);
    if (!thread->managed){
        LT_MutexWord_unlock(&thread->state_lock);
        LT_error("Cannot check finished status of unmanaged thread");
    }
    finished = thread->finished;
    LT_MutexWord_unlock(&thread->state_lock);
    return finished;
}
bool LT_Thread_joined_p(LT_Thread* thread){
    int joined;

    LT_MutexWord_lock(&thread->state_lock);
    if (!thread->managed){
        LT_MutexWord_unlock(&thread->state_lock);
        LT_error("Cannot check joined status of unmanaged thread");
    }
    joined = thread->joined;
    LT_MutexWord_unlock(&thread->state_lock);
    return joined;
}
bool LT_Thread_detached_p(LT_Thread* thread){
    int detached;

    LT_MutexWord_lock(&thread->state_lock);
    if (!thread->managed){
        LT_MutexWord_unlock(&thread->state_lock);
        LT_error("Cannot check detached status of unmanaged thread");
    }
    detached = thread->detached;
    LT_MutexWord_unlock(&thread->state_lock);
    return detached;
}
LT_Value LT_Thread_result(LT_Thread* thread){
    LT_Value result;

    LT_MutexWord_lock(&thread->state_lock);
    if (!thread->managed){
        LT_MutexWord_unlock(&thread->state_lock);
        LT_error("Cannot get result of unmanaged thread");
    }
    if (!thread->finished){
        LT_MutexWord_unlock(&thread->state_lock);
        LT_error("Thread has not finished yet");
    }
    result = thread->result;
    LT_MutexWord_unlock(&thread->state_lock);
    return result;
}