/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/vm/thread_state.h>
#include <ListTalk/vm/error.h>

#include <signal.h>
#include <stdatomic.h>
#include <string.h>

_Thread_local LT_ThreadState* LT__thread_state = NULL;

#if defined(NSIG)
#define LT_POSIX_SIGNAL_COUNT NSIG
#elif defined(_NSIG)
#define LT_POSIX_SIGNAL_COUNT _NSIG
#else
#define LT_POSIX_SIGNAL_COUNT 128
#endif

static _Atomic LT_Value LT_posix_signal_callables[LT_POSIX_SIGNAL_COUNT];
static struct sigaction LT_posix_signal_previous_actions[LT_POSIX_SIGNAL_COUNT];
static int LT_posix_signal_registered[LT_POSIX_SIGNAL_COUNT];

static pthread_key_t LT_thread_state_key;
static pthread_once_t LT_thread_state_key_once = PTHREAD_ONCE_INIT;

static void LT_thread_state_destroy(void* state)
{
    if (LT__thread_state == state){
        LT__thread_state = NULL;
    }
    GC_FREE(state);
}

static void LT_thread_state_make_key(void)
{
    pthread_key_create(&LT_thread_state_key, LT_thread_state_destroy);
}

static void LT_posix_signal_handler(int signal_number)
{
    LT_ThreadState* state = LT__thread_state;
    LT_Value callable;

    if (signal_number <= 0 || signal_number >= LT_POSIX_SIGNAL_COUNT){
        return;
    }
    if (state == NULL){
        return;
    }

    callable = atomic_load_explicit(
        &LT_posix_signal_callables[signal_number],
        memory_order_acquire
    );
    if (callable != LT_INVALID){
        atomic_store_explicit(
            &state->pending_signal,
            callable,
            memory_order_release
        );
    }
}

static void validate_posix_signal_number(int signal_number)
{
    if (signal_number <= 0 || signal_number >= LT_POSIX_SIGNAL_COUNT){
        LT_error("Invalid POSIX signal number");
    }
}

void LT_register_posix_signal(int signal_number, LT_Value callable)
{
    struct sigaction action;

    validate_posix_signal_number(signal_number);
    if (callable == LT_INVALID){
        LT_error("Cannot register LT_INVALID as POSIX signal callable");
    }

    memset(&action, 0, sizeof(action));
    action.sa_handler = LT_posix_signal_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (!LT_posix_signal_registered[signal_number]){
        if (sigaction(
                signal_number,
                NULL,
                &LT_posix_signal_previous_actions[signal_number]
            ) != 0){
            LT_error("Could not inspect POSIX signal action");
        }
    }

    atomic_store_explicit(
        &LT_posix_signal_callables[signal_number],
        callable,
        memory_order_release
    );
    if (sigaction(signal_number, &action, NULL) != 0){
        atomic_store_explicit(
            &LT_posix_signal_callables[signal_number],
            LT_INVALID,
            memory_order_release
        );
        LT_error("Could not register POSIX signal action");
    }
    LT_posix_signal_registered[signal_number] = 1;
}

void LT_unregister_posix_signal(int signal_number)
{
    validate_posix_signal_number(signal_number);

    if (!LT_posix_signal_registered[signal_number]){
        return;
    }

    atomic_store_explicit(
        &LT_posix_signal_callables[signal_number],
        LT_INVALID,
        memory_order_release
    );
    if (sigaction(
            signal_number,
            &LT_posix_signal_previous_actions[signal_number],
            NULL
        ) != 0){
        LT_error("Could not unregister POSIX signal action");
    }
    LT_posix_signal_registered[signal_number] = 0;
}

LT_ThreadState* LT_thread_state_slow(void)
{
    LT_ThreadState* state;

    pthread_once(&LT_thread_state_key_once, LT_thread_state_make_key);

    state = pthread_getspecific(LT_thread_state_key);
    if (state == NULL){
        state = GC_MALLOC_UNCOLLECTABLE(sizeof(LT_ThreadState));
        memset(state, 0, sizeof(LT_ThreadState));
        atomic_init(&state->pending_signal, LT_INVALID);
        pthread_setspecific(LT_thread_state_key, state);
    }

    LT__thread_state = state;
    return state;
}
