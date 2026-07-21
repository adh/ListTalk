/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>

#include <stdio.h>

typedef struct CondThreadArgs CondThreadArgs;

struct CondThreadArgs {
    LT_Mutex* mutex;
    LT_ConditionVariable* cond;
    LT_ConditionVariable* ready_cond;
    int* value;
    int* waiting_count;
    int* woken_count;
};

static int fail(const char* message)
{
    fprintf(stderr, "%s\n", message);
    return 1;
}

static void* cond_thread_main(void* opaque)
{
    CondThreadArgs* args = opaque;

    LT_Mutex_lock(args->mutex);
    *args->waiting_count = 1;
    LT_ConditionVariable_signal(args->ready_cond);

    while (*args->value == 0){
        LT_ConditionVariable_wait(args->cond, args->mutex);
    }
    *args->woken_count = 1;
    LT_Mutex_unlock(args->mutex);

    return NULL;
}

static int test_read_write_lock(void)
{
    LT_ReadWriteLock* lock = LT_ReadWriteLock_new();

    if (!LT_ReadWriteLock_tryReadLock(lock)){
        return fail("tryReadLock failed on unlocked lock");
    }
    if (LT_ReadWriteLock_tryWriteLock(lock)){
        return fail("tryWriteLock succeeded while read locked");
    }
    LT_ReadWriteLock_readUnlock(lock);

    if (!LT_ReadWriteLock_tryWriteLock(lock)){
        return fail("tryWriteLock failed on unlocked lock");
    }
    if (LT_ReadWriteLock_tryReadLock(lock)){
        return fail("tryReadLock succeeded while write locked");
    }
    LT_ReadWriteLock_writeUnlock(lock);

    return 0;
}

static int test_condition_variable(void)
{
    LT_Mutex* mutex = LT_Mutex_new();
    LT_ConditionVariable* cond = LT_ConditionVariable_new();
    LT_ConditionVariable* ready_cond = LT_ConditionVariable_new();
    pthread_t thread;
    CondThreadArgs args;
    int value = 0;
    int waiting_count = 0;
    int woken_count = 0;

    args.mutex = mutex;
    args.cond = cond;
    args.ready_cond = ready_cond;
    args.value = &value;
    args.waiting_count = &waiting_count;
    args.woken_count = &woken_count;

    if (pthread_create(&thread, NULL, cond_thread_main, &args) != 0){
        return fail("pthread_create failed");
    }

    LT_Mutex_lock(mutex);
    while (waiting_count == 0){
        LT_ConditionVariable_wait(ready_cond, mutex);
    }
    value = 1;
    LT_ConditionVariable_signal(cond);
    LT_ConditionVariable_broadcast(cond);
    LT_Mutex_unlock(mutex);

    if (pthread_join(thread, NULL) != 0){
        return fail("pthread_join failed");
    }
    if (woken_count != 1){
        return fail("condition variable failed to wake waiter");
    }

    return 0;
}

int main(void)
{
    LT_INIT();

    if (test_read_write_lock() != 0){
        return 1;
    }
    if (test_condition_variable() != 0){
        return 1;
    }

    return 0;
}
