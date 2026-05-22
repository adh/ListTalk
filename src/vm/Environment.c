/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/vm/Environment.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/InvocationContextKind.h>
#include <ListTalk/classes/BindingDescriptor.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/utils.h>

#include <stddef.h>
#include <stdint.h>

struct LT_Environment_Binding {
    LT_Value value;
    unsigned int flags;
};

struct LT_Environment_s {
    LT_Object base;
    LT_Environment* parent;
    LT_InlineHash bindings;
    unsigned int frame_flags;
    LT_Value invocation_context_kind;
    LT_Value invocation_context_data;
};

LT_InvocationContextKind LT_send_invocation_context = {
    .base = {.klass = &LT_InvocationContextKind_class},
};

static LT_Slot_Descriptor Environment_slots[] = {
    {"parent", offsetof(LT_Environment, parent), &LT_SlotType_ReadonlyObject},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static void Environment_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Environment* environment = LT_Environment_from_value(obj);
    fprintf(stream, "#<Environment %p>", (void*)environment);
}

LT_DECLARE_PRIMITIVE(
    environment_method_bindings_do,
    "Environment>>bindingsDo:",
    "(self callable)",
    "Call callable for each direct binding reflection."
);
LT_DECLARE_PRIMITIVE(
    environment_method_bindings_as_list,
    "Environment>>bindingsAsList",
    "(self)",
    "Return direct binding reflections as a list."
);

static LT_Method_Descriptor Environment_methods[] = {
    {"bindingsDo:", &environment_method_bindings_do},
    {"bindingsAsList", &environment_method_bindings_as_list},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Environment) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Environment",
    .documentation = "Mapping from names to values during evaluation.",
    .instance_size = sizeof(LT_Environment),
    .debugPrintOn = Environment_debugPrintOn,
    .slots = Environment_slots,
    .methods = Environment_methods,
};

static void* environment_symbol_key(LT_Value symbol){
    return (void*)(symbol);
}

LT_Environment* LT_Environment_new(LT_Environment* parent,
                                   LT_Value invocation_context_kind,
                                   LT_Value invocation_context_data){
    LT_Environment* environment = LT_Class_ALLOC(LT_Environment);
    environment->parent = parent;
    environment->frame_flags = 0;
    environment->invocation_context_kind = invocation_context_kind;
    environment->invocation_context_data = invocation_context_data;
    LT_InlineHash_init(&environment->bindings);
    return environment;
}

LT_Environment* LT_Environment_parent(LT_Environment* environment){
    return environment->parent;
}

LT_Value LT_Environment_invocation_context_of_kind(
    LT_Environment* environment,
    LT_Value invocation_context_kind
){
    if (invocation_context_kind != LT_NIL
        && !LT_InvocationContextKind_p(invocation_context_kind)){
        LT_type_error(invocation_context_kind, &LT_InvocationContextKind_class);
    }

    while (environment != NULL){
        if (environment->invocation_context_kind == invocation_context_kind){
            return environment->invocation_context_data;
        }
        environment = environment->parent;
    }

    return LT_NIL;
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

static void environment_binding_do(LT_Value binding, LT_Value callable){
    (void)LT_apply(callable, LT_cons(binding, LT_NIL), LT_NIL, LT_NIL, NULL);
}

static void Environment_bindings_do(LT_Environment* environment, LT_Value callable){
    LT_InlineHash* table = &environment->bindings;
    size_t i;

    for (i = 0; i < table->mask + 1; i++){
        LT_InlineHash_Entry* table_entry = table->vector[i];

        while (table_entry != NULL){
            struct LT_Environment_Binding* entry =
                (struct LT_Environment_Binding*)table_entry->value;
            LT_Value symbol = (LT_Value)(uintptr_t)table_entry->key;
            LT_Value binding = LT_BindingDescriptor_new(
                symbol,
                entry->value,
                entry->flags
            );

            environment_binding_do(binding, callable);
            table_entry = table_entry->next;
        }
    }
}

static LT_Value Environment_bindings_as_list(LT_Environment* environment){
    LT_InlineHash* table = &environment->bindings;
    LT_ListBuilder* builder = LT_ListBuilder_new();
    size_t i;

    for (i = 0; i < table->mask + 1; i++){
        LT_InlineHash_Entry* table_entry = table->vector[i];

        while (table_entry != NULL){
            struct LT_Environment_Binding* entry =
                (struct LT_Environment_Binding*)table_entry->value;
            LT_Value symbol = (LT_Value)(uintptr_t)table_entry->key;
            LT_ListBuilder_append(
                builder,
                LT_BindingDescriptor_new(symbol, entry->value, entry->flags)
            );
            table_entry = table_entry->next;
        }
    }

    return LT_ListBuilder_value(builder);
}

LT_PRIMITIVE_HEAD(environment_method_bindings_do){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);

    Environment_bindings_do(LT_Environment_from_value(self), callable);
    return LT_NIL;
}

LT_PRIMITIVE_HEAD(environment_method_bindings_as_list){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return Environment_bindings_as_list(LT_Environment_from_value(self));
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
