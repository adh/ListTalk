/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/vm/Environment.h>
#include <ListTalk/utils.h>

struct LT_Environment_Binding {
    LT_Value value;
    unsigned int flags;
};

struct LT_Environment {
    LT_Environment* parent;
    LT_InlineHash bindings;
    unsigned int frame_flags;
    unsigned int invocation_context_kind;
    void* invocation_context_data;
};

static void* environment_symbol_key(LT_Value symbol){
    return LT_VALUE_POINTER_VALUE(symbol);
}

LT_Environment* LT_Environment_new(LT_Environment* parent){
    LT_Environment* environment = GC_NEW(LT_Environment);
    environment->parent = parent;
    environment->frame_flags = 0;
    environment->invocation_context_kind = 0;
    environment->invocation_context_data = NULL;
    LT_InlineHash_init(&environment->bindings);
    return environment;
}

LT_Environment* LT_Environment_parent(LT_Environment* environment){
    return environment->parent;
}

void LT_Environment_bind(LT_Environment* environment,
                         LT_Value symbol,
                         LT_Value value,
                         unsigned int flags){
    struct LT_Environment_Binding* binding;
    void* symbol_key;

    symbol_key = environment_symbol_key(symbol);
    binding = LT_PointerHash_at(&environment->bindings, symbol_key);

    if (binding == NULL){
        binding = GC_NEW(struct LT_Environment_Binding);
        LT_PointerHash_at_put(&environment->bindings, symbol_key, binding);
    }

    binding->value = value;
    binding->flags = flags;
}

int LT_Environment_lookup(LT_Environment* environment,
                          LT_Value symbol,
                          LT_Value* value_out,
                          unsigned int* flags_out){
    void* symbol_key = environment_symbol_key(symbol);

    while (environment != NULL){
        struct LT_Environment_Binding* binding;

        binding = LT_PointerHash_at(&environment->bindings, symbol_key);
        if (binding != NULL){
            if (value_out != NULL){
                *value_out = binding->value;
            }
            if (flags_out != NULL){
                *flags_out = binding->flags;
            }
            return 1;
        }

        environment = environment->parent;
    }

    return 0;
}

int LT_Environment_set(LT_Environment* environment,
                       LT_Value symbol,
                       LT_Value value){
    void* symbol_key = environment_symbol_key(symbol);

    while (environment != NULL){
        struct LT_Environment_Binding* binding;

        binding = LT_PointerHash_at(&environment->bindings, symbol_key);
        if (binding != NULL){
            if ((binding->flags & LT_ENV_BINDING_FLAG_CONSTANT) != 0){
                return 0;
            }
            binding->value = value;
            return 1;
        }

        environment = environment->parent;
    }

    return 0;
}
