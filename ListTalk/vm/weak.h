/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__weak__
#define H__ListTalk__weak__

#include <ListTalk/vm/value.h>

LT__BEGIN_DECLS

typedef struct LT_WeakValue_s {
    uint64_t masked_value;
} LT_WeakValue;

typedef struct LT_WeakRead_s {
    LT_WeakValue* value;
    LT_Value unboxed;
    bool alive;
} LT_WeakRead;

static inline void* LT_weak_try_unbox_locked(void* opaque){
    LT_WeakRead* read = opaque;

    if (read->value->masked_value == 0){
        read->alive = false;
        read->unboxed = LT_NIL;
    } else {
        read->alive = true;
        read->unboxed = (LT_Value)~read->value->masked_value;
    }

    return NULL;
}

static inline void LT_weak_box(LT_WeakValue* destination, LT_Value value){
    void** link = (void**)(void*)&destination->masked_value;

    GC_unregister_disappearing_link(link);
    destination->masked_value = ~(uint64_t)value;

    if (LT_VALUE_IS_POINTER(value)){
        void* pointer = LT_VALUE_POINTER_VALUE(value);
        void* base = GC_base(pointer);

        if (base != NULL){
            GC_general_register_disappearing_link(link, base);
        }
    }
}

static inline bool LT_weak_try_unbox(LT_WeakValue* value, LT_Value* value_out){
    LT_WeakRead read;

    read.value = value;
    read.unboxed = LT_NIL;
    read.alive = false;
    GC_call_with_alloc_lock(LT_weak_try_unbox_locked, &read);
    if (read.alive && value_out != NULL){
        *value_out = read.unboxed;
    }
    return read.alive;
}

static inline LT_Value LT_weak_unbox(LT_WeakValue* value){
    LT_Value unboxed;

    return LT_weak_try_unbox(value, &unboxed) ? unboxed : LT_INVALID2;
}

static inline bool LT_weak_is_alive(LT_WeakValue* value){
    return LT_weak_try_unbox(value, NULL);
}

LT__END_DECLS

#endif
