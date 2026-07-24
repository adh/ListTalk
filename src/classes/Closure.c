/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Closure.h>
#include <ListTalk/classes/Function.h>
#include <ListTalk/classes/List.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/macros/method_macros.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/compiler.h>
#include <ListTalk/vm/epoch.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/utils.h>

#include <inttypes.h>
#include <stddef.h>
#include <string.h>

struct LT_Closure_s {
    LT_Value name;
    LT_Value parameters;
    LT_Value body;
    LT_Value compiled_body;
    LT_EpochCounter compilation_epoch;
    LT_Value documentation;
    LT_Environment* environment;
};

static LT_Slot_Descriptor Closure_slots[] = {
    {"name", offsetof(LT_Closure, name), &LT_SlotType_ReadonlyObject},
    {"parameters", offsetof(LT_Closure, parameters), &LT_SlotType_ReadonlyObject},
    {"body", offsetof(LT_Closure, body), &LT_SlotType_ReadonlyObject},
    {"compiled-body", offsetof(LT_Closure, compiled_body), &LT_SlotType_ReadonlyObject},
    {"documentation", offsetof(LT_Closure, documentation), &LT_SlotType_ReadonlyObject},
    {"environment", offsetof(LT_Closure, environment), &LT_SlotType_ReadonlyObject},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static LT_Value compile_closure_body(LT_Value body,
                                     LT_Environment* environment){
    LT_ListBuilder* builder = LT_ListBuilder_new();
    LT_Value cursor = body;

    while (cursor != LT_NIL){
        if (!LT_Pair_p(cursor)){
            LT_error("Closure body expects proper list of forms");
        }
        LT_ListBuilder_append(
            builder,
            LT_compiler_fold_expression(LT_car(cursor), environment)
        );
        cursor = LT_cdr(cursor);
    }

    return LT_ImmutableList_fromList(LT_ListBuilder_value(builder));
}

static int keyword_marker_p(LT_Value value, const char* name){
    if (!LT_Symbol_p(value)){
        return 0;
    }
    if (LT_Symbol_package(LT_Symbol_from_value(value)) != LT_PACKAGE_KEYWORD){
        return 0;
    }
    return strcmp(LT_Symbol_name(LT_Symbol_from_value(value)), name) == 0;
}

static LT_Value defaultable_parameter_name(LT_Value parameter){
    if (LT_Symbol_p(parameter)){
        return parameter;
    }
    if (LT_Pair_p(parameter)){
        return LT_car(parameter);
    }
    return LT_NIL;
}

static void bind_compiler_parameter_shadow(LT_Environment* environment,
                                           LT_Value symbol){
    if (LT_Symbol_p(symbol)){
        LT_Environment_bind(environment, symbol, LT_NIL, 0);
    }
}

static void bind_compiler_parameter_shadows(LT_Environment* environment,
                                            LT_Value parameters){
    LT_Value cursor = parameters;
    int optional_parameters = 0;

    if (LT_Symbol_p(cursor)){
        bind_compiler_parameter_shadow(environment, cursor);
        return;
    }

    while (LT_Pair_p(cursor)){
        LT_Value parameter = LT_car(cursor);

        if (keyword_marker_p(parameter, "optional")){
            optional_parameters = 1;
            cursor = LT_cdr(cursor);
            continue;
        }

        if (keyword_marker_p(parameter, "key")){
            cursor = LT_cdr(cursor);
            while (LT_Pair_p(cursor)){
                bind_compiler_parameter_shadow(
                    environment,
                    defaultable_parameter_name(LT_car(cursor))
                );
                cursor = LT_cdr(cursor);
            }
            return;
        }

        if (keyword_marker_p(parameter, "rest")){
            LT_Value tail = LT_cdr(cursor);
            if (LT_Pair_p(tail)){
                bind_compiler_parameter_shadow(environment, LT_car(tail));
            }
            return;
        }

        if (optional_parameters){
            bind_compiler_parameter_shadow(
                environment,
                defaultable_parameter_name(parameter)
            );
        } else {
            bind_compiler_parameter_shadow(environment, parameter);
        }

        cursor = LT_cdr(cursor);
    }

    if (LT_Symbol_p(cursor)){
        bind_compiler_parameter_shadow(environment, cursor);
    }
}

static void compile_closure(LT_Closure* closure){
    LT_EpochCounter compilation_epoch = 0;
    LT_Environment* compilation_environment;

    LT_compilation_epoch_copy_acquire_release(&compilation_epoch);
    compilation_environment = LT_Environment_new(
        closure->environment,
        LT_NIL,
        LT_NIL
    );
    bind_compiler_parameter_shadows(
        compilation_environment,
        closure->parameters
    );
    closure->compiled_body = compile_closure_body(
        closure->body,
        compilation_environment
    );
    LT_epoch_copy_acquire_release(
        &closure->compilation_epoch,
        &compilation_epoch
    );
}

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

LT_DEFINE_PRIMITIVE(
    closure_method_documentation,
    "Closure>>documentation",
    "(self)",
    "Return closure documentation."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Closure_documentation(LT_Closure_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    closure_method_arguments,
    "Closure>>arguments",
    "(self)",
    "Return closure parameter list."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Closure_parameters(LT_Closure_from_value(self));
}

static LT_Method_Descriptor Closure_methods[] = {
    {"documentation", &closure_method_documentation},
    {"arguments", &closure_method_arguments},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Closure) {
    .superclass = &LT_Function_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Closure",
    .documentation = "Callable function object with captured environment.",
    .instance_size = sizeof(LT_Closure),
    .class_flags = LT_CLASS_FLAG_SPECIAL,
    .debugPrintOn = Closure_debugPrintOn,
    .slots = Closure_slots,
    .methods = Closure_methods,
};

LT_Value LT_Closure_new(LT_Value name,
                        LT_Value parameters,
                        LT_Value body,
                        LT_Environment* environment){
    LT_Value documentation = LT_NIL;

    if (LT_Pair_p(body)
        && LT_Pair_p(LT_cdr(body))
        && LT_String_p(LT_car(body))){
        documentation = LT_car(body);
        body = LT_cdr(body);
    }

    return LT_Closure_new_with_documentation(
        name,
        parameters,
        body,
        environment,
        documentation
    );
}

LT_Value LT_Closure_new_with_documentation(LT_Value name,
                                           LT_Value parameters,
                                           LT_Value body,
                                           LT_Environment* environment,
                                           LT_Value documentation){
    LT_Closure* closure = GC_NEW(LT_Closure);

    if (documentation != LT_NIL && !LT_String_p(documentation)){
        LT_type_error(documentation, &LT_String_class);
    }

    closure->name = name;
    closure->parameters = parameters;
    closure->body = body;
    closure->documentation = documentation;
    closure->environment = environment;
    compile_closure(closure);
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

LT_Value LT_Closure_compiled_body(LT_Closure* closure){
    if (!LT_compilation_epoch_equals_acquire(&closure->compilation_epoch)){
        compile_closure(closure);
    }
    return closure->compiled_body;
}

int LT_Closure_compilation_epoch_current_p(LT_Closure* closure){
    return LT_compilation_epoch_equals_acquire(&closure->compilation_epoch);
}

LT_Value LT_Closure_documentation(LT_Closure* closure){
    return closure->documentation;
}

LT_Environment* LT_Closure_environment(LT_Closure* closure){
    return closure->environment;
}
