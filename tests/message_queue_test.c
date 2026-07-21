/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>

#include <stdio.h>

typedef struct QueueThreadArgs QueueThreadArgs;

struct QueueThreadArgs {
    LT_MessageQueue* queue;
    LT_Value value;
};

static int fail(const char* message)
{
    fprintf(stderr, "%s\n", message);
    return 1;
}

static void* producer_thread_main(void* opaque)
{
    QueueThreadArgs* args = opaque;

    LT_MessageQueue_put(args->queue, args->value);
    return NULL;
}

static void* consumer_thread_main(void* opaque)
{
    QueueThreadArgs* args = opaque;

    args->value = LT_MessageQueue_get(args->queue);
    return NULL;
}

static int test_try_put_get(void)
{
    LT_MessageQueue* queue = LT_MessageQueue_new(2);
    LT_Value value;

    if (LT_MessageQueue_capacity(queue) != 2){
        return fail("queue capacity is incorrect");
    }
    if (!LT_MessageQueue_empty_p(queue)){
        return fail("new queue is not empty");
    }
    if (!LT_MessageQueue_tryPut(queue, LT_SmallInteger_new(1))){
        return fail("tryPut failed on empty queue");
    }
    if (!LT_MessageQueue_tryPut(queue, LT_SmallInteger_new(2))){
        return fail("tryPut failed before capacity");
    }
    if (!LT_MessageQueue_full_p(queue)){
        return fail("queue is not full");
    }
    if (LT_MessageQueue_tryPut(queue, LT_SmallInteger_new(3))){
        return fail("tryPut succeeded on full queue");
    }
    if (!LT_MessageQueue_tryGet(queue, &value) ||
        value != LT_SmallInteger_new(1)){
        return fail("tryGet did not return first value");
    }
    if (!LT_MessageQueue_tryGet(queue, &value) ||
        value != LT_SmallInteger_new(2)){
        return fail("tryGet did not return second value");
    }
    if (LT_MessageQueue_tryGet(queue, &value)){
        return fail("tryGet succeeded on empty queue");
    }

    return 0;
}

static int test_capacity_one_blocks_until_space(void)
{
    LT_MessageQueue* queue = LT_MessageQueue_new(1);
    pthread_t producer;
    QueueThreadArgs args;
    LT_Value value;

    LT_MessageQueue_put(queue, LT_SmallInteger_new(10));

    args.queue = queue;
    args.value = LT_SmallInteger_new(20);
    if (pthread_create(&producer, NULL, producer_thread_main, &args) != 0){
        return fail("pthread_create failed");
    }

    if (!LT_MessageQueue_full_p(queue)){
        return fail("capacity-one queue is not full");
    }
    value = LT_MessageQueue_get(queue);
    if (value != LT_SmallInteger_new(10)){
        return fail("capacity-one queue returned wrong first value");
    }

    if (pthread_join(producer, NULL) != 0){
        return fail("pthread_join failed");
    }
    value = LT_MessageQueue_get(queue);
    if (value != LT_SmallInteger_new(20)){
        return fail("capacity-one queue returned wrong second value");
    }

    return 0;
}

static int test_get_blocks_until_value(void)
{
    LT_MessageQueue* queue = LT_MessageQueue_new(1);
    pthread_t consumer;
    QueueThreadArgs args;

    args.queue = queue;
    args.value = LT_NIL;
    if (pthread_create(&consumer, NULL, consumer_thread_main, &args) != 0){
        return fail("pthread_create failed");
    }

    LT_MessageQueue_put(queue, LT_SmallInteger_new(30));

    if (pthread_join(consumer, NULL) != 0){
        return fail("pthread_join failed");
    }
    if (args.value != LT_SmallInteger_new(30)){
        return fail("blocking get returned wrong value");
    }

    return 0;
}

int main(void)
{
    LT_INIT();

    if (test_try_put_get() != 0){
        return 1;
    }
    if (test_capacity_one_blocks_until_space() != 0){
        return 1;
    }
    if (test_get_blocks_until_value() != 0){
        return 1;
    }

    return 0;
}
