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

static inline LT_Value LT_weak_unbox(LT_WeakValue value){
    return (LT_Value)~value.masked_value;
}

static inline bool LT_weak_is_alive(LT_WeakValue value){
    return value.masked_value != NULL;
}

LT__END_DECLS

#endif
