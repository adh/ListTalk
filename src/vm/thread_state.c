/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/vm/thread_state.h>

#include <string.h>

_Thread_local LT_ThreadState* LT__thread_state = NULL;

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

LT_ThreadState* LT_thread_state_slow(void)
{
    LT_ThreadState* state;

    pthread_once(&LT_thread_state_key_once, LT_thread_state_make_key);

    state = pthread_getspecific(LT_thread_state_key);
    if (state == NULL){
        state = GC_MALLOC_UNCOLLECTABLE(sizeof(LT_ThreadState));
        memset(state, 0, sizeof(LT_ThreadState));
        pthread_setspecific(LT_thread_state_key, state);
    }

    LT__thread_state = state;
    return state;
}
