/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Function.h>
#include <ListTalk/classes/List.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Reader.h>
#include <ListTalk/classes/Restart.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/Class.h>

#include <stddef.h>
#include <stdint.h>

static LT_Slot_Descriptor Restart_slots[] = {
    {"name", offsetof(LT_Restart, name), &LT_SlotType_ReadonlyObject},
    {"description", offsetof(LT_Restart, description), &LT_SlotType_ReadonlyObject},
    {"argument-list", offsetof(LT_Restart, argument_list), &LT_SlotType_ReadonlyObject},
    {"callable", offsetof(LT_Restart, callable), &LT_SlotType_ReadonlyObject},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static LT_Value restart_string_or_nil(char* string){
    if (string == NULL){
        return LT_NIL;
    }
    return (LT_Value)(uintptr_t)LT_String_new_cstr(string);
}

static LT_Value restart_argument_list_or_nil(char* arguments_text){
    LT_Reader* reader;
    LT_ReaderStream* stream;
    LT_Value argument_list;

    if (arguments_text == NULL){
        return LT_NIL;
    }

    reader = LT_Reader_new(LT_NIL);
    stream = LT_ReaderStream_newForString(arguments_text);
    argument_list = LT_Reader_readObject(reader, stream);
    if (!LT_List_proper_p(argument_list)){
        LT_error("Static restart argument-list must be a proper list");
    }
    return argument_list;
}

static void Restart_materialize_static(LT_Restart* restart){
    LT_Primitive* primitive;

    if (restart->name != LT_INVALID){
        return;
    }

    primitive = LT_Primitive_from_value(restart->callable);
    restart->name = restart_string_or_nil(LT_Primitive_name(primitive));
    restart->description = restart_string_or_nil(LT_Primitive_description(primitive));
    restart->argument_list = restart_argument_list_or_nil(
        LT_Primitive_arguments(primitive)
    );
}

static void Restart_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Restart* restart = LT_Restart_from_value(obj);

    Restart_materialize_static(restart);
    fputs("#<Restart ", stream);
    LT_Value_debugPrintOn(restart->name, stream);
    fputc('>', stream);
}

LT_DEFINE_PRIMITIVE(
    restart_method_name,
    "Restart>>name",
    "(self)",
    "Return restart name."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Restart_name(LT_Restart_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    restart_method_description,
    "Restart>>description",
    "(self)",
    "Return restart description."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Restart_description(LT_Restart_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    restart_method_argument_list,
    "Restart>>argument-list",
    "(self)",
    "Return restart argument list."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Restart_argument_list(LT_Restart_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    restart_method_callable,
    "Restart>>callable",
    "(self)",
    "Return restart callable."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Restart_callable(LT_Restart_from_value(self));
}

static LT_Method_Descriptor Restart_methods[] = {
    {"name", &restart_method_name},
    {"description", &restart_method_description},
    {"argument-list", &restart_method_argument_list},
    {"callable", &restart_method_callable},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Restart) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Restart",
    .documentation = "Immutable description of a restart and its callable implementation.",
    .instance_size = sizeof(LT_Restart),
    .class_flags = LT_CLASS_FLAG_FINAL | LT_CLASS_FLAG_IMMUTABLE,
    .debugPrintOn = Restart_debugPrintOn,
    .slots = Restart_slots,
    .methods = Restart_methods,
};

LT_Value LT_Restart_new(LT_Value name,
                        LT_Value description,
                        LT_Value argument_list,
                        LT_Value callable){
    LT_Restart* restart;

    if (name != LT_NIL && !LT_Symbol_p(name) && !LT_String_p(name)){
        LT_error("Restart name must be a symbol, string, or nil");
    }
    if (description != LT_NIL && !LT_String_p(description)){
        LT_type_error(description, &LT_String_class);
    }
    if (!LT_List_proper_p(argument_list)){
        LT_error("Restart argument-list must be a proper list");
    }
    if (!LT_Value_is_instance_of(callable, LT_STATIC_CLASS(LT_Function))){
        LT_error("Restart callable must be a function");
    }

    restart = LT_Class_ALLOC(LT_Restart);
    restart->name = name;
    restart->description = description;
    restart->argument_list = argument_list;
    restart->callable = callable;
    return (LT_Value)(uintptr_t)restart;
}

LT_Value LT_Restart_from_static(LT_Restart* restart){
    Restart_materialize_static(restart);
    return (LT_Value)(uintptr_t)restart;
}

void LT_Restart_init_static(LT_Restart* restart, LT_Primitive* primitive){
    restart->callable = LT_Primitive_from_static(primitive);
    restart->name = restart_string_or_nil(LT_Primitive_name(primitive));
    restart->description = restart_string_or_nil(LT_Primitive_description(primitive));
    restart->argument_list = restart_argument_list_or_nil(
        LT_Primitive_arguments(primitive)
    );
}

LT_Value LT_Restart_name(LT_Restart* restart){
    Restart_materialize_static(restart);
    return restart->name;
}

LT_Value LT_Restart_description(LT_Restart* restart){
    Restart_materialize_static(restart);
    return restart->description;
}

LT_Value LT_Restart_argument_list(LT_Restart* restart){
    Restart_materialize_static(restart);
    return restart->argument_list;
}

LT_Value LT_Restart_callable(LT_Restart* restart){
    Restart_materialize_static(restart);
    return restart->callable;
}
