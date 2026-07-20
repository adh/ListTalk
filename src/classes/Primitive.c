/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Function.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Reader.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/conditions.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/thread_state.h>
#include <ListTalk/utils.h>

static LT_Value primitive_reader_error_handler_impl(
    LT_Value arguments,
    LT_Value invocation_context_kind,
    LT_Value invocation_context_data,
    LT_TailCallUnwindMarker* tail_call_unwind_marker
){
    LT_Value cursor = arguments;
    LT_Value condition;
    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, condition);
    LT_ARG_END(cursor);
    (void)condition;
    LT_throw(LT_thread_state()->primitive_reader_error_tag, LT_TRUE);
}

static LT_Primitive primitive_reader_error_handler = {
    .function = primitive_reader_error_handler_impl,
    .flags = 0,
    .name = "primitive-reader-error-handler",
    .arguments = "(condition)",
    .description = "Catch reader errors while parsing primitive metadata."
};

static void Primitive_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Primitive* primitive = LT_Primitive_from_value(obj);
    if (primitive->name == NULL){
        fputs("#<Primitive>", stream);
        return;
    }
    fputs("#<Primitive ", stream);
    fputs(primitive->name, stream);
    fputc('>', stream);
}

static LT_Value primitive_string_or_nil(char* string){
    if (string == NULL){
        return LT_NIL;
    }
    return (LT_Value)(uintptr_t)LT_String_new_cstr(string);
}

static LT_Value primitive_parse_arguments_or_string(char* arguments_text){
    LT_Value caught = LT_NIL;
    LT_Value parsed = LT_NIL;
    LT_Reader* reader;
    LT_ReaderStream* stream;

    if (arguments_text == NULL){
        return LT_NIL;
    }

    reader = LT_Reader_new(LT_NIL);
    stream = LT_ReaderStream_newForString(arguments_text);
    LT_thread_state()->primitive_reader_error_tag =
        LT_Symbol_new("primitive-reader-error");
    LT_CATCH(LT_thread_state()->primitive_reader_error_tag, caught, {
        LT_HANDLER_BIND(LT_Primitive_from_static(&primitive_reader_error_handler), {
            parsed = LT_Reader_readObject(reader, stream);
        });
    });

    if (caught != LT_NIL){
        return (LT_Value)(uintptr_t)LT_String_new_cstr(arguments_text);
    }
    return parsed;
}

LT_DEFINE_PRIMITIVE(
    primitive_method_documentation,
    "Primitive>>documentation",
    "(self)",
    "Return primitive documentation."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return primitive_string_or_nil(
        LT_Primitive_description(LT_Primitive_from_value(self))
    );
}

LT_DEFINE_PRIMITIVE(
    primitive_method_arguments,
    "Primitive>>arguments",
    "(self)",
    "Return primitive argument list, or the raw argument string if it cannot be read."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return primitive_parse_arguments_or_string(
        LT_Primitive_arguments(LT_Primitive_from_value(self))
    );
}

static LT_Method_Descriptor Primitive_methods[] = {
    {"documentation", &primitive_method_documentation},
    {"arguments", &primitive_method_arguments},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Primitive) {
    .superclass = &LT_Function_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Primitive",
    .documentation = "Callable function implemented in native code.",
    .instance_size = sizeof(LT_Primitive),
    .class_flags = LT_CLASS_FLAG_SPECIAL,
    .debugPrintOn = Primitive_debugPrintOn,
    .methods = Primitive_methods,
};

LT_Value LT_Primitive_new(char* name,
                          char* arguments,
                          char* description,
                          LT_Primitive_Func function){
    return LT_Primitive_new_with_flags(
        name,
        arguments,
        description,
        function,
        0
    );
}

LT_Value LT_Primitive_new_with_flags(char* name,
                                     char* arguments,
                                     char* description,
                                     LT_Primitive_Func function,
                                     unsigned int flags){
    LT_Primitive* primitive = GC_NEW(LT_Primitive);

    primitive->function = function;
    primitive->flags = flags;
    if (name == NULL){
        primitive->name = NULL;
    } else {
        primitive->name = LT_strdup(name);
    }
    if (arguments == NULL){
        primitive->arguments = NULL;
    } else {
        primitive->arguments = LT_strdup(arguments);
    }
    if (description == NULL){
        primitive->description = NULL;
    } else {
        primitive->description = LT_strdup(description);
    }

    return ((LT_Value)(uintptr_t)primitive) | LT_VALUE_POINTER_TAG_PRIMITIVE;
}

LT_Value LT_Primitive_from_static(LT_Primitive* primitive){
    return ((LT_Value)(uintptr_t)primitive) | LT_VALUE_POINTER_TAG_PRIMITIVE;
}

char* LT_Primitive_name(LT_Primitive* primitive){
    return primitive->name;
}

char* LT_Primitive_arguments(LT_Primitive* primitive){
    return primitive->arguments;
}

char* LT_Primitive_description(LT_Primitive* primitive){
    return primitive->description;
}

LT_Primitive_Func LT_Primitive_function(LT_Primitive* primitive){
    return primitive->function;
}

unsigned int LT_Primitive_flags(LT_Primitive* primitive){
    return primitive->flags;
}

LT_Value LT_Primitive_call(LT_Value primitive,
                           LT_Value arguments,
                           LT_Value invocation_context_kind,
                           LT_Value invocation_context_data,
                           LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_Primitive_Func function = LT_Primitive_function(
        LT_Primitive_from_value(primitive)
    );
    return function(
        arguments,
        invocation_context_kind,
        invocation_context_data,
        tail_call_unwind_marker
    );
}
