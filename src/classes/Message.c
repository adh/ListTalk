/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Message.h>
#include <ListTalk/classes/Object.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/Class.h>

#include <stddef.h>

struct LT_Message_s {
    LT_Object base;
    LT_Value selector;
    LT_Value receiver;
    LT_Value arguments;
};

static LT_Slot_Descriptor Message_slots[] = {
    {"selector", offsetof(LT_Message, selector), &LT_SlotType_ReadonlyObject},
    {"receiver", offsetof(LT_Message, receiver), &LT_SlotType_ReadonlyObject},
    {"arguments", offsetof(LT_Message, arguments), &LT_SlotType_ReadonlyObject},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static void Message_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Message* message = LT_Message_from_value(obj);

    fputs("#<Message selector=", stream);
    LT_Value_debugPrintOn(message->selector, stream);
    fputs(" receiver=", stream);
    LT_Value_debugPrintOn(message->receiver, stream);
    fputs(" arguments=", stream);
    LT_Value_debugPrintOn(message->arguments, stream);
    fputc('>', stream);
}

LT_DEFINE_PRIMITIVE(
    message_method_selector,
    "Message>>selector",
    "(self)",
    "Return message selector."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Message_selector(LT_Message_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    message_method_receiver,
    "Message>>receiver",
    "(self)",
    "Return message receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Message_receiver(LT_Message_from_value(self));
}

LT_DEFINE_PRIMITIVE(
    message_method_arguments,
    "Message>>arguments",
    "(self)",
    "Return message arguments."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Message_arguments(LT_Message_from_value(self));
}

static LT_Method_Descriptor Message_methods[] = {
    {"selector", &message_method_selector},
    {"receiver", &message_method_receiver},
    {"arguments", &message_method_arguments},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Message) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Message",
    .documentation = "Object message selector and arguments.",
    .instance_size = sizeof(LT_Message),
    .debugPrintOn = Message_debugPrintOn,
    .slots = Message_slots,
    .methods = Message_methods,
};

LT_Value LT_Message_new(
    LT_Value selector,
    LT_Value receiver,
    LT_Value arguments
){
    LT_Message* message = LT_Class_ALLOC(LT_Message);
    message->selector = selector;
    message->receiver = receiver;
    message->arguments = arguments;
    return (LT_Value)(uintptr_t)message;
}

LT_Value LT_Message_selector(LT_Message* message){
    return message->selector;
}

LT_Value LT_Message_receiver(LT_Message* message){
    return message->receiver;
}

LT_Value LT_Message_arguments(LT_Message* message){
    return message->arguments;
}
