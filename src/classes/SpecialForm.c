/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Function.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Reader.h>
#include <ListTalk/classes/SpecialForm.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/conditions.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/utils.h>

static _Thread_local LT_Value special_form_reader_error_tag = LT_NIL;

static LT_Value special_form_reader_error_handler_impl(
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
    LT_throw(special_form_reader_error_tag, LT_TRUE);
}

static LT_Primitive special_form_reader_error_handler = {
    .function = special_form_reader_error_handler_impl,
    .flags = 0,
    .name = "special-form-reader-error-handler",
    .arguments = "(condition)",
    .description = "Catch reader errors while parsing special form metadata."
};

static void SpecialForm_debugPrintOn(LT_Value obj, FILE* stream){
    LT_SpecialForm* special_form = LT_SpecialForm_from_value(obj);
    if (special_form->name == NULL){
        fputs("#<SpecialForm>", stream);
        return;
    }
    fputs("#<SpecialForm ", stream);
    fputs(special_form->name, stream);
    fputc('>', stream);
}

static LT_Value special_form_string_or_nil(char* string){
    if (string == NULL){
        return LT_NIL;
    }
    return (LT_Value)(uintptr_t)LT_String_new_cstr(string);
}

static LT_Value special_form_parse_arguments_or_string(char* arguments_text){
    LT_Value caught = LT_NIL;
    LT_Value parsed = LT_NIL;
    LT_Reader* reader;
    LT_ReaderStream* stream;

    if (arguments_text == NULL){
        return LT_NIL;
    }

    reader = LT_Reader_new(LT_NIL);
    stream = LT_ReaderStream_newForString(arguments_text);
    special_form_reader_error_tag = LT_Symbol_new("special-form-reader-error");
    LT_CATCH(special_form_reader_error_tag, caught, {
        LT_HANDLER_BIND(LT_Primitive_from_static(&special_form_reader_error_handler), {
            parsed = LT_Reader_readObject(reader, stream);
        });
    });

    if (caught != LT_NIL){
        return (LT_Value)(uintptr_t)LT_String_new_cstr(arguments_text);
    }
    return parsed;
}

LT_DEFINE_PRIMITIVE(
    special_form_method_documentation,
    "SpecialForm>>documentation",
    "(self)",
    "Return special form documentation."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return special_form_string_or_nil(
        LT_SpecialForm_description(LT_SpecialForm_from_value(self))
    );
}

LT_DEFINE_PRIMITIVE(
    special_form_method_arguments,
    "SpecialForm>>arguments",
    "(self)",
    "Return special form argument list, or the raw argument string if it cannot be read."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return special_form_parse_arguments_or_string(
        LT_SpecialForm_arguments(LT_SpecialForm_from_value(self))
    );
}

static LT_Method_Descriptor SpecialForm_methods[] = {
    {"documentation", &special_form_method_documentation},
    {"arguments", &special_form_method_arguments},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_SpecialForm) {
    .superclass = &LT_Function_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "SpecialForm",
    .documentation = "Evaluator form with special argument rules.",
    .instance_size = sizeof(LT_SpecialForm),
    .class_flags = LT_CLASS_FLAG_SPECIAL,
    .debugPrintOn = SpecialForm_debugPrintOn,
    .methods = SpecialForm_methods,
};

LT_Value LT_SpecialForm_new(char* name,
                            char* arguments,
                            char* description,
                            LT_SpecialForm_Func function,
                            LT_SpecialForm_ExpandFunc expand_function){
    LT_SpecialForm* special_form = GC_NEW(LT_SpecialForm);

    special_form->function = function;
    special_form->expand_function = expand_function;
    if (name == NULL){
        special_form->name = NULL;
    } else {
        special_form->name = LT_strdup(name);
    }
    if (arguments == NULL){
        special_form->arguments = NULL;
    } else {
        special_form->arguments = LT_strdup(arguments);
    }
    if (description == NULL){
        special_form->description = NULL;
    } else {
        special_form->description = LT_strdup(description);
    }

    return ((LT_Value)(uintptr_t)special_form) | LT_VALUE_POINTER_TAG_SPECIAL_FORM;
}

LT_Value LT_SpecialForm_from_static(LT_SpecialForm* special_form){
    return ((LT_Value)(uintptr_t)special_form) | LT_VALUE_POINTER_TAG_SPECIAL_FORM;
}

char* LT_SpecialForm_name(LT_SpecialForm* special_form){
    return special_form->name;
}

char* LT_SpecialForm_arguments(LT_SpecialForm* special_form){
    return special_form->arguments;
}

char* LT_SpecialForm_description(LT_SpecialForm* special_form){
    return special_form->description;
}

LT_SpecialForm_Func LT_SpecialForm_function(LT_SpecialForm* special_form){
    return special_form->function;
}

LT_SpecialForm_ExpandFunc LT_SpecialForm_expand_function(
    LT_SpecialForm* special_form
){
    return special_form->expand_function;
}

LT_Value LT_SpecialForm_apply(LT_Value special_form,
                              LT_Value arguments,
                              LT_Environment* environment,
                              LT_TailCallUnwindMarker* tail_call_unwind_marker){
    LT_SpecialForm_Func function = LT_SpecialForm_function(
        LT_SpecialForm_from_value(special_form)
    );
    return function(arguments, environment, tail_call_unwind_marker);
}

LT_Value LT_SpecialForm_expand(LT_Value special_form,
                               LT_Value form,
                               LT_Environment* environment){
    LT_SpecialForm_ExpandFunc function = LT_SpecialForm_expand_function(
        LT_SpecialForm_from_value(special_form)
    );

    if (function == NULL){
        return LT_INVALID;
    }
    return function(form, environment);
}
