/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifdef __linux__
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

#include <ListTalk/classes/Thread.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/ListTalk.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/thread_state.h>

#include <errno.h>
#include <assert.h>

struct LT_Thread_s {
    LT_Object base;
    pthread_t pthread;
    LT_MutexWord state_lock;
    LT_CondWord state_cond;
    LT_ThreadState* thread_state;
    LT_Value callable;
    LT_Value result;
    char* name;
    bool finished : 1;
    bool joined : 1;
    bool joining : 1;
    bool detached : 1;
    bool managed : 1;
};

static void Thread_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Thread* thread = LT_Thread_from_value(obj);

    if (thread->name != NULL){
        fprintf(stream, "#<Thread %s %p>", thread->name, (void*)thread);
    } else {
        fprintf(stream, "#<Thread %p>", (void*)thread);
    }
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
    return (LT_Value)(uintptr_t)LT_Thread_new(callable, NULL);
}

LT_DEFINE_PRIMITIVE(
    thread_class_method_new_named,
    "Thread class>>new:named:",
    "(self callable name)",
    "Start callable in a new named thread and return the thread."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    LT_String* name;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_GENERIC_ARG(cursor, name, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_Thread_class){
        LT_error("new:named: class method is only supported on Thread");
    }
    return (LT_Value)(uintptr_t)LT_Thread_new(
        callable,
        (char*)LT_String_value_cstr(name)
    );
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

LT_DEFINE_PRIMITIVE(
    thread_method_name,
    "Thread>>name",
    "(self)",
    "Return thread name, or nil when it has none."
){
    LT_Value cursor = arguments;
    LT_Value self;
    char* name;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    name = LT_Thread_name(LT_Thread_from_value(self));
    if (name == NULL){
        return LT_NIL;
    }
    return (LT_Value)(uintptr_t)LT_String_new_cstr(name);
}

static LT_Method_Descriptor Thread_methods[] = {
    {"join", &thread_method_join},
    {"makeDetached", &thread_method_make_detached},
    {"makeDaemon", &thread_method_make_detached},
    {"detached?", &thread_method_detached_p},
    {"finished?", &thread_method_finished_p},
    {"joined?", &thread_method_joined_p},
    {"result", &thread_method_result},
    {"name", &thread_method_name},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor Thread_class_methods[] = {
    {"new:", &thread_class_method_new},
    {"new:named:", &thread_class_method_new_named},
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

static void set_current_native_thread_name(char* name){
    if (name == NULL || name[0] == '\0'){
        return;
    }

#ifdef __APPLE__
    (void)pthread_setname_np(name);
#elif defined(__linux__)
    (void)pthread_setname_np(pthread_self(), name);
#endif
}

static void* thread_main(void* opaque){
    LT_Thread* thread = opaque;
    LT_ThreadState* state = LT_thread_state();
    LT_Value result;

    LT_MutexWord_lock(&thread->state_lock);
    thread->thread_state = state;
    LT_MutexWord_unlock(&thread->state_lock);

    state->current_thread = thread;
    set_current_native_thread_name(thread->name);
    result = LT_apply(thread->callable, LT_NIL, LT_NIL, LT_NIL, NULL);

    LT_MutexWord_lock(&thread->state_lock);
    thread->result = result;
    thread->finished = 1;
    thread->thread_state = NULL;
    LT_MutexWord_unlock(&thread->state_lock);

    return NULL;
}

LT_Thread* LT_Thread_new(LT_Value callable, char* name){
    LT_Thread* thread = LT_Class_ALLOC(LT_Thread);
    int errnum;

    LT_MutexWord_init(&thread->state_lock);
    LT_CondWord_init(&thread->state_cond);
    thread->thread_state = NULL;
    thread->callable = callable;
    thread->result = LT_NIL;
    thread->name = name != NULL ? LT_strdup(name) : NULL;
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
    LT_ThreadState* state = LT_thread_state();

    if (state->current_thread == NULL){
        state->current_thread = LT_Class_ALLOC(LT_Thread);
        state->current_thread->pthread = pthread_self();
        LT_MutexWord_init(&state->current_thread->state_lock);
        LT_CondWord_init(&state->current_thread->state_cond);
        state->current_thread->thread_state = state;
        state->current_thread->callable = LT_NIL;
        state->current_thread->result = LT_NIL;
        state->current_thread->name = NULL;
        state->current_thread->finished = 1;
        state->current_thread->joined = 1;
        state->current_thread->joining = 0;
        state->current_thread->detached = 0;
        state->current_thread->managed = 0;
    }
    return state->current_thread;
}
    
char* LT_Thread_name(LT_Thread* thread){
    return thread->name;
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
    while (thread->joining){
        LT_CondWord_wait(&thread->state_cond, &thread->state_lock);
    }
    if (thread->joined){
        result = thread->result;
        LT_MutexWord_unlock(&thread->state_lock);
        return result;
    }
    thread->joining = 1;
    LT_MutexWord_unlock(&thread->state_lock);

    errnum = pthread_join(thread->pthread, NULL);
    if (errnum != 0){
        LT_MutexWord_lock(&thread->state_lock);
        thread->joining = 0;
        LT_CondWord_broadcast(&thread->state_cond);
        LT_MutexWord_unlock(&thread->state_lock);
        LT_system_error("Could not join thread", errnum);
    }

    LT_MutexWord_lock(&thread->state_lock);
    assert(thread->finished);
    thread->joined = 1;
    thread->joining = 0;
    result = thread->result;
    LT_CondWord_broadcast(&thread->state_cond);

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
    if (thread->joining){
        LT_MutexWord_unlock(&thread->state_lock);
        LT_error("Cannot make thread daemon while it is being joined");
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
