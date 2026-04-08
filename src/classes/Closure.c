/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Closure.h>
#include <ListTalk/vm/Class.h>

#include <inttypes.h>
#include <stddef.h>

struct LT_Closure_s {
    LT_Value name;
    LT_Value parameters;
    LT_Value body;
    LT_Environment* environment;
};

static LT_Slot_Descriptor Closure_slots[] = {
    {"name", offsetof(LT_Closure, name), &LT_SlotType_ReadonlyObject},
    {"parameters", offsetof(LT_Closure, parameters), &LT_SlotType_ReadonlyObject},
    {"body", offsetof(LT_Closure, body), &LT_SlotType_ReadonlyObject},
    {"environment", offsetof(LT_Closure, environment), &LT_SlotType_ReadonlyObject},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static void Closure_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Closure* closure = LT_Closure_from_value(obj);

    if (closure->name != LT_NIL){
        fputs("#<Closure ", stream);
        LT_Value_debugPrintOn(closure->name, stream);
        fputc('>', stream);
        return;
    }
    fprintf(stream, "#<Closure at 0x%" PRIxPTR ">", (uintptr_t)closure);
}

LT_DEFINE_CLASS(LT_Closure) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Closure",
    .instance_size = sizeof(LT_Closure),
    .class_flags = LT_CLASS_FLAG_SPECIAL,
    .debugPrintOn = Closure_debugPrintOn,
    .slots = Closure_slots,
};

LT_Value LT_Closure_new(LT_Value name,
                        LT_Value parameters,
                        LT_Value body,
                        LT_Environment* environment){
    LT_Closure* closure = GC_NEW(LT_Closure);
    closure->name = name;
    closure->parameters = parameters;
    closure->body = body;
    closure->environment = environment;
    return ((LT_Value)(uintptr_t)closure) | LT_VALUE_POINTER_TAG_CLOSURE;
}

LT_Value LT_Closure_invocation_context_of_kind(
    LT_Closure* closure,
    LT_Value invocation_context_kind
){
    return LT_Environment_invocation_context_of_kind(
        closure->environment,
        invocation_context_kind
    );
}

LT_Value LT_Closure_name(LT_Closure* closure){
    return closure->name;
}

LT_Value LT_Closure_parameters(LT_Closure* closure){
    return closure->parameters;
}

LT_Value LT_Closure_body(LT_Closure* closure){
    return closure->body;
}

LT_Environment* LT_Closure_environment(LT_Closure* closure){
    return closure->environment;
}
