/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/MessageQueue.h>
#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/utils/lock.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>

#include <stdint.h>

struct LT_MessageQueue_s {
    LT_Object base;
    LT_MutexWord mutex;
    LT_CondWord not_empty;
    LT_CondWord not_full;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    LT_Value name;
    LT_Value items[];
};

static size_t next_index(LT_MessageQueue* queue, size_t index){
    index++;
    if (index == queue->capacity){
        return 0;
    }
    return index;
}

static void MessageQueue_debugPrintOn(LT_Value obj, FILE* stream){
    LT_MessageQueue* queue = LT_MessageQueue_from_value(obj);

    if (queue->name != LT_NIL){
        fputs("#<MessageQueue ", stream);
        LT_Value_debugPrintOn(queue->name, stream);
        fprintf(
            stream,
            " %p size=%zu capacity=%zu>",
            (void*)queue,
            LT_MessageQueue_size(queue),
            queue->capacity
        );
    } else {
        fprintf(
            stream,
            "#<MessageQueue %p size=%zu capacity=%zu>",
            (void*)queue,
            LT_MessageQueue_size(queue),
            queue->capacity
        );
    }
}

LT_DEFINE_PRIMITIVE(
    message_queue_class_method_new,
    "MessageQueue class>>new:",
    "(self capacity)",
    "Return a new message queue with fixed capacity."
){
    LT_Value cursor = arguments;
    LT_Value self;
    int64_t capacity;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_FIXNUM_ARG(cursor, capacity);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_MessageQueue_class){
        LT_error("new: class method is only supported on MessageQueue");
    }
    if (capacity <= 0){
        LT_error("MessageQueue capacity must be positive");
    }
    return (LT_Value)(uintptr_t)LT_MessageQueue_new((size_t)capacity);
}

LT_DEFINE_PRIMITIVE(
    message_queue_class_method_new_named,
    "MessageQueue class>>new:named:",
    "(self capacity name)",
    "Return a new named message queue with fixed capacity."
){
    LT_Value cursor = arguments;
    LT_Value self;
    int64_t capacity;
    LT_Value name;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_FIXNUM_ARG(cursor, capacity);
    LT_OBJECT_ARG(cursor, name);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_MessageQueue_class){
        LT_error("new:named: class method is only supported on MessageQueue");
    }
    if (capacity <= 0){
        LT_error("MessageQueue capacity must be positive");
    }
    return (LT_Value)(uintptr_t)LT_MessageQueue_new_named(
        (size_t)capacity,
        name
    );
}

LT_DEFINE_PRIMITIVE(
    message_queue_method_capacity,
    "MessageQueue>>capacity",
    "(self)",
    "Return message queue capacity."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_SmallInteger_new(
        (int64_t)LT_MessageQueue_capacity(LT_MessageQueue_from_value(self))
    );
}

LT_DEFINE_PRIMITIVE(
    message_queue_method_size,
    "MessageQueue>>size",
    "(self)",
    "Return number of messages currently in queue."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_SmallInteger_new(
        (int64_t)LT_MessageQueue_size(LT_MessageQueue_from_value(self))
    );
}

LT_DEFINE_PRIMITIVE(
    message_queue_method_empty_p,
    "MessageQueue>>empty?",
    "(self)",
    "Return true when queue contains no messages."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_MessageQueue_empty_p(LT_MessageQueue_from_value(self))
        ? LT_TRUE
        : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    message_queue_method_full_p,
    "MessageQueue>>full?",
    "(self)",
    "Return true when queue cannot accept another message without blocking."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_MessageQueue_full_p(LT_MessageQueue_from_value(self))
        ? LT_TRUE
        : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    message_queue_method_put,
    "MessageQueue>>put:",
    "(self value)",
    "Put value into queue, blocking while queue is full, and return receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    LT_MessageQueue_put(LT_MessageQueue_from_value(self), value);
    return self;
}

LT_DEFINE_PRIMITIVE(
    message_queue_method_try_put,
    "MessageQueue>>tryPut:",
    "(self value)",
    "Try to put value into queue and return whether it was enqueued."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    return LT_MessageQueue_tryPut(LT_MessageQueue_from_value(self), value)
        ? LT_TRUE
        : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    message_queue_method_get,
    "MessageQueue>>get",
    "(self)",
    "Get next value from queue, blocking while queue is empty."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_MessageQueue_get(LT_MessageQueue_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    message_queue_method_try_get,
    "MessageQueue>>tryGet",
    "(self)",
    "Try to get next value from queue, or nil when queue is empty."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    if (!LT_MessageQueue_tryGet(LT_MessageQueue_from_value(self), &value)){
        return LT_NIL;
    }
    return value;
}

LT_DEFINE_PRIMITIVE(
    message_queue_method_name,
    "MessageQueue>>name",
    "(self)",
    "Return message queue name, or nil when it has none."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    return LT_MessageQueue_name(LT_MessageQueue_from_value(self));
}

static LT_Method_Descriptor MessageQueue_methods[] = {
    {"capacity", &message_queue_method_capacity},
    {"size", &message_queue_method_size},
    {"empty?", &message_queue_method_empty_p},
    {"full?", &message_queue_method_full_p},
    {"put:", &message_queue_method_put},
    {"tryPut:", &message_queue_method_try_put},
    {"get", &message_queue_method_get},
    {"tryGet", &message_queue_method_try_get},
    {"name", &message_queue_method_name},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor MessageQueue_class_methods[] = {
    {"new:", &message_queue_class_method_new},
    {"new:named:", &message_queue_class_method_new_named},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_MessageQueue) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "MessageQueue",
    .documentation = "Fixed-size blocking queue of object values.",
    .instance_size = sizeof(LT_MessageQueue),
    .class_flags = LT_CLASS_FLAG_FLEXIBLE,
    .debugPrintOn = MessageQueue_debugPrintOn,
    .methods = MessageQueue_methods,
    .class_methods = MessageQueue_class_methods,
};

LT_MessageQueue* LT_MessageQueue_new(size_t capacity){
    return LT_MessageQueue_new_named(capacity, LT_NIL);
}

LT_MessageQueue* LT_MessageQueue_new_named(size_t capacity, LT_Value name){
    LT_MessageQueue* queue;
    size_t i;

    if (capacity == 0){
        LT_error("MessageQueue capacity must be positive");
    }
    if (capacity > SIZE_MAX / sizeof(LT_Value)){
        LT_error("MessageQueue capacity is too large");
    }

    queue = LT_Class_ALLOC_FLEXIBLE(
        LT_MessageQueue,
        sizeof(LT_Value) * capacity
    );
    LT_MutexWord_init(&queue->mutex);
    LT_CondWord_init(&queue->not_empty);
    LT_CondWord_init(&queue->not_full);
    queue->capacity = capacity;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->name = name;
    for (i = 0; i < capacity; i++){
        queue->items[i] = LT_NIL;
    }
    return queue;
}

size_t LT_MessageQueue_capacity(LT_MessageQueue* queue){
    return queue->capacity;
}

size_t LT_MessageQueue_size(LT_MessageQueue* queue){
    size_t count;

    LT_MutexWord_lock(&queue->mutex);
    count = queue->count;
    LT_MutexWord_unlock(&queue->mutex);
    return count;
}

int LT_MessageQueue_empty_p(LT_MessageQueue* queue){
    return LT_MessageQueue_size(queue) == 0;
}

int LT_MessageQueue_full_p(LT_MessageQueue* queue){
    size_t count;

    LT_MutexWord_lock(&queue->mutex);
    count = queue->count;
    LT_MutexWord_unlock(&queue->mutex);
    return count == queue->capacity;
}

void LT_MessageQueue_put(LT_MessageQueue* queue, LT_Value value){
    LT_MutexWord_lock(&queue->mutex);
    while (queue->count == queue->capacity){
        LT_CondWord_wait(&queue->not_full, &queue->mutex);
    }

    queue->items[queue->tail] = value;
    queue->tail = next_index(queue, queue->tail);
    queue->count++;

    LT_CondWord_signal(&queue->not_empty);
    LT_MutexWord_unlock(&queue->mutex);
}

int LT_MessageQueue_tryPut(LT_MessageQueue* queue, LT_Value value){
    if (!LT_MutexWord_try_lock(&queue->mutex)){
        return 0;
    }
    if (queue->count == queue->capacity){
        LT_MutexWord_unlock(&queue->mutex);
        return 0;
    }

    queue->items[queue->tail] = value;
    queue->tail = next_index(queue, queue->tail);
    queue->count++;

    LT_CondWord_signal(&queue->not_empty);
    LT_MutexWord_unlock(&queue->mutex);
    return 1;
}

LT_Value LT_MessageQueue_get(LT_MessageQueue* queue){
    LT_Value value;

    LT_MutexWord_lock(&queue->mutex);
    while (queue->count == 0){
        LT_CondWord_wait(&queue->not_empty, &queue->mutex);
    }

    value = queue->items[queue->head];
    queue->items[queue->head] = LT_NIL;
    queue->head = next_index(queue, queue->head);
    queue->count--;

    LT_CondWord_signal(&queue->not_full);
    LT_MutexWord_unlock(&queue->mutex);
    return value;
}

int LT_MessageQueue_tryGet(LT_MessageQueue* queue, LT_Value* value_out){
    LT_Value value;

    if (!LT_MutexWord_try_lock(&queue->mutex)){
        return 0;
    }
    if (queue->count == 0){
        LT_MutexWord_unlock(&queue->mutex);
        return 0;
    }

    value = queue->items[queue->head];
    queue->items[queue->head] = LT_NIL;
    queue->head = next_index(queue, queue->head);
    queue->count--;

    LT_CondWord_signal(&queue->not_full);
    LT_MutexWord_unlock(&queue->mutex);
    if (value_out != NULL){
        *value_out = value;
    }
    return 1;
}

LT_Value LT_MessageQueue_name(LT_MessageQueue* queue){
    return queue->name;
}
