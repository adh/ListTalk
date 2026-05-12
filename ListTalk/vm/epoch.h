/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__epoch__
#define H__ListTalk__epoch__

#include <ListTalk/macros/env_macros.h>

#include <stdatomic.h>
#include <stdbool.h>

LT__BEGIN_DECLS

typedef uint64_t LT_EpochValue;
typedef _Atomic(LT_EpochValue) LT_EpochCounter;

extern LT_EpochCounter LT__compilation_epoch;
extern LT_EpochCounter ilc_epoch;

static inline LT_EpochValue LT_epoch_increment(LT_EpochCounter* epoch){
    return atomic_fetch_add_explicit(epoch, 1, memory_order_acq_rel) + 1;
}

static inline LT_EpochValue LT_compilation_epoch_increment(void){
    return LT_epoch_increment(&LT__compilation_epoch);
}

static inline LT_EpochValue LT_ilc_epoch_increment(void){
    return LT_epoch_increment(&ilc_epoch);
}

static inline bool LT_epoch_equals_acquire(
    LT_EpochCounter* left,
    LT_EpochCounter* right
){
    LT_EpochValue left_value = atomic_load_explicit(left, memory_order_acquire);
    LT_EpochValue right_value = atomic_load_explicit(right, memory_order_acquire);

    return left_value == right_value;
}

static inline bool LT_compilation_epoch_equals_acquire(LT_EpochCounter* epoch){
    return LT_epoch_equals_acquire(&LT__compilation_epoch, epoch);
}

static inline bool LT_ilc_epoch_equals_acquire(LT_EpochCounter* epoch){
    return LT_epoch_equals_acquire(&ilc_epoch, epoch);
}

static inline void LT_epoch_copy_acquire_release(
    LT_EpochCounter* destination,
    LT_EpochCounter* source
){
    LT_EpochValue value = atomic_load_explicit(source, memory_order_acquire);
    atomic_store_explicit(destination, value, memory_order_release);
}

static inline void LT_compilation_epoch_copy_acquire_release(LT_EpochCounter* destination){
    LT_epoch_copy_acquire_release(destination, &LT__compilation_epoch);
}

static inline void LT_ilc_epoch_copy_acquire_release(LT_EpochCounter* destination){
    LT_epoch_copy_acquire_release(destination, &ilc_epoch);
}

LT__END_DECLS

#endif
