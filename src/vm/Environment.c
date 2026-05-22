/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/vm/Environment.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/ListTalk.h>
#include <ListTalk/vm/base_env.h>
#include <ListTalk/classes/InvocationContextKind.h>
#include <ListTalk/classes/Symbol.h>
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
    environment_class_method_new,
    "Environment class>>new",
    "(self)",
    "Return a new empty environment."
);
LT_DECLARE_PRIMITIVE(
    environment_class_method_new_with_parent,
    "Environment class>>newWithParent:",
    "(self parent)",
    "Return a new environment with parent."
);
LT_DECLARE_PRIMITIVE(
    environment_class_method_shared_base,
    "Environment class>>sharedBase",
    "(self)",
    "Return the shared base environment."
);
LT_DECLARE_PRIMITIVE(
    environment_class_method_new_base,
    "Environment class>>newBase",
    "(self)",
    "Return a new base environment."
);
LT_DECLARE_PRIMITIVE(
    environment_method_contains,
    "Environment>>contains?:",
    "(self symbol)",
    "Return true when symbol is bound in this environment or its parents."
);
LT_DECLARE_PRIMITIVE(
    environment_method_at,
    "Environment>>at:",
    "(self symbol)",
    "Return symbol value, or nil when symbol is unbound."
);
LT_DECLARE_PRIMITIVE(
    environment_method_at_put,
    "Environment>>at:put:",
    "(self symbol value)",
    "Bind symbol to value in this environment and return value."
);
LT_DECLARE_PRIMITIVE(
    environment_method_at_put_constant,
    "Environment>>at:putConstant:",
    "(self symbol value)",
    "Bind symbol to a constant value in this environment and return value."
);
LT_DECLARE_PRIMITIVE(
    environment_method_constant,
    "Environment>>constant?:",
    "(self symbol)",
    "Return true when symbol resolves to a constant binding."
);
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
    {"contains?:", &environment_method_contains},
    {"at:", &environment_method_at},
    {"at:put:", &environment_method_at_put},
    {"at:putConstant:", &environment_method_at_put_constant},
    {"constant?:", &environment_method_constant},
    {"bindingsDo:", &environment_method_bindings_do},
    {"bindingsAsList", &environment_method_bindings_as_list},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor Environment_class_methods[] = {
    {"new", &environment_class_method_new},
    {"newWithParent:", &environment_class_method_new_with_parent},
    {"sharedBase", &environment_class_method_shared_base},
    {"newBase", &environment_class_method_new_base},
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
    .class_methods = Environment_class_methods,
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

static LT_Value environment_binding_constant_p(unsigned int flags){
    return (flags & LT_ENV_BINDING_FLAG_CONSTANT) != 0 ? LT_TRUE : LT_FALSE;
}

LT_PRIMITIVE_HEAD(environment_class_method_new){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    (void)LT_Class_from_object(self);
    return (LT_Value)(uintptr_t)LT_Environment_new(NULL, LT_NIL, LT_NIL);
}

LT_PRIMITIVE_HEAD(environment_class_method_new_with_parent){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value parent_value;
    LT_Environment* parent;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, parent_value);
    LT_ARG_END(cursor);
    (void)LT_Class_from_object(self);
    parent = LT_Environment_from_value(parent_value);
    return (LT_Value)(uintptr_t)LT_Environment_new(parent, LT_NIL, LT_NIL);
}

LT_PRIMITIVE_HEAD(environment_class_method_shared_base){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    (void)LT_Class_from_object(self);
    return (LT_Value)(uintptr_t)LT_get_shared_base_environment();
}

LT_PRIMITIVE_HEAD(environment_class_method_new_base){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    (void)LT_Class_from_object(self);
    return (LT_Value)(uintptr_t)LT_new_base_environment();
}

LT_PRIMITIVE_HEAD(environment_method_contains){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value symbol;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, symbol);
    LT_ARG_END(cursor);
    if (!LT_Symbol_p(symbol)){
        LT_type_error(symbol, &LT_Symbol_class);
    }
    return LT_Environment_lookup(
        LT_Environment_from_value(self),
        symbol,
        NULL,
        NULL
    ) ? LT_TRUE : LT_FALSE;
}

LT_PRIMITIVE_HEAD(environment_method_at){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value symbol;
    LT_Value value = LT_NIL;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, symbol);
    LT_ARG_END(cursor);
    if (!LT_Symbol_p(symbol)){
        LT_type_error(symbol, &LT_Symbol_class);
    }
    if (!LT_Environment_lookup(
        LT_Environment_from_value(self),
        symbol,
        &value,
        NULL
    )){
        LT_error("Environment binding not found");
    }
    return value;
}

LT_PRIMITIVE_HEAD(environment_method_at_put){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value symbol;
    LT_Value value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, symbol);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    if (!LT_Symbol_p(symbol)){
        LT_type_error(symbol, &LT_Symbol_class);
    }
    LT_Environment_bind(LT_Environment_from_value(self), symbol, value, 0);
    return value;
}

LT_PRIMITIVE_HEAD(environment_method_at_put_constant){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value symbol;
    LT_Value value;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, symbol);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    if (!LT_Symbol_p(symbol)){
        LT_type_error(symbol, &LT_Symbol_class);
    }
    LT_Environment_bind(
        LT_Environment_from_value(self),
        symbol,
        value,
        LT_ENV_BINDING_FLAG_CONSTANT
    );
    return value;
}

LT_PRIMITIVE_HEAD(environment_method_constant){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value symbol;
    unsigned int flags = 0;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, symbol);
    LT_ARG_END(cursor);
    if (!LT_Symbol_p(symbol)){
        LT_type_error(symbol, &LT_Symbol_class);
    }
    if (!LT_Environment_lookup(
        LT_Environment_from_value(self),
        symbol,
        NULL,
        &flags
    )){
        return LT_FALSE;
    }
    return environment_binding_constant_p(flags);
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
