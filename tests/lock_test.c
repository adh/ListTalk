/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/utils/lock.h>

#include <stdio.h>

enum {
    THREAD_COUNT = 8,
    ITERATION_COUNT = 20000,
};

typedef struct MutexThreadArgs MutexThreadArgs;
typedef struct RWLockThreadArgs RWLockThreadArgs;

struct MutexThreadArgs {
    LT_MutexWord* mutex;
    int* value;
};

struct RWLockThreadArgs {
    LT_RWLockWord* lock;
    int* value;
};

static int fail(const char* message)
{
    fprintf(stderr, "%s\n", message);
    return 1;
}

static void* mutex_thread_main(void* opaque)
{
    MutexThreadArgs* args = opaque;

    for (int i = 0; i < ITERATION_COUNT; ++i){
        LT_MutexWord_lock(args->mutex);
        ++*args->value;
        LT_MutexWord_unlock(args->mutex);
    }

    return NULL;
}

static void* rwlock_thread_main(void* opaque)
{
    RWLockThreadArgs* args = opaque;

    for (int i = 0; i < ITERATION_COUNT; ++i){
        LT_RWLockWord_write_lock(args->lock);
        ++*args->value;
        LT_RWLockWord_write_unlock(args->lock);

        LT_RWLockWord_read_lock(args->lock);
        if (*args->value < 0){
            LT_RWLockWord_read_unlock(args->lock);
            return (void*) 1;
        }
        LT_RWLockWord_read_unlock(args->lock);
    }

    return NULL;
}

static int test_mutex_contention(void)
{
    LT_MutexWord mutex = LT_MUTEX_INITIALIZER;
    pthread_t threads[THREAD_COUNT];
    MutexThreadArgs args;
    int value = 0;

    args.mutex = &mutex;
    args.value = &value;

    for (int i = 0; i < THREAD_COUNT; ++i){
        if (pthread_create(&threads[i], NULL, mutex_thread_main, &args) != 0){
            return fail("pthread_create failed");
        }
    }

    for (int i = 0; i < THREAD_COUNT; ++i){
        if (pthread_join(threads[i], NULL) != 0){
            return fail("pthread_join failed");
        }
    }

    if (value != THREAD_COUNT * ITERATION_COUNT){
        return fail("mutex lost increments");
    }

    return 0;
}

static int test_mutex_try_lock(void)
{
    LT_MutexWord mutex;

    LT_MutexWord_init(&mutex);

    if (!LT_MutexWord_try_lock(&mutex)){
        return fail("try_lock failed on unlocked mutex");
    }
    if (LT_MutexWord_try_lock(&mutex)){
        return fail("try_lock succeeded on locked mutex");
    }
    LT_MutexWord_unlock(&mutex);

    return 0;
}

static int test_rwlock_contention(void)
{
    LT_RWLockWord lock = LT_RWLOCK_INITIALIZER;
    pthread_t threads[THREAD_COUNT];
    RWLockThreadArgs args;
    int value = 0;

    args.lock = &lock;
    args.value = &value;

    for (int i = 0; i < THREAD_COUNT; ++i){
        if (pthread_create(&threads[i], NULL, rwlock_thread_main, &args) != 0){
            return fail("pthread_create failed");
        }
    }

    for (int i = 0; i < THREAD_COUNT; ++i){
        void* result = NULL;

        if (pthread_join(threads[i], &result) != 0){
            return fail("pthread_join failed");
        }
        if (result != NULL){
            return fail("rwlock reader observed invalid value");
        }
    }

    if (value != THREAD_COUNT * ITERATION_COUNT){
        return fail("rwlock lost increments");
    }

    return 0;
}

static int test_rwlock_try_lock(void)
{
    LT_RWLockWord lock;

    LT_RWLockWord_init(&lock);

    if (!LT_RWLockWord_try_read_lock(&lock)){
        return fail("try_read_lock failed on unlocked lock");
    }
    if (LT_RWLockWord_try_write_lock(&lock)){
        return fail("try_write_lock succeeded while read locked");
    }
    LT_RWLockWord_read_unlock(&lock);

    if (!LT_RWLockWord_try_write_lock(&lock)){
        return fail("try_write_lock failed on unlocked lock");
    }
    if (LT_RWLockWord_try_read_lock(&lock)){
        return fail("try_read_lock succeeded while write locked");
    }
    LT_RWLockWord_write_unlock(&lock);

    return 0;
}

int main(void)
{
    if (test_mutex_try_lock() != 0){
        return 1;
    }
    if (test_mutex_contention() != 0){
        return 1;
    }
    if (test_rwlock_try_lock() != 0){
        return 1;
    }
    if (test_rwlock_contention() != 0){
        return 1;
    }

    return 0;
}
