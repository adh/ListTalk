/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/StackFrame.h>
#include <ListTalk/vm/Class.h>

#include <stddef.h>

struct LT_StackFrameObject_s {
    LT_Object base;
};

struct LT_EvalStackFrame_s {
    LT_StackFrameObject base;
    LT_Value expression;
    LT_Environment* environment;
};

struct LT_ApplyStackFrame_s {
    LT_StackFrameObject base;
    LT_Value callable;
    LT_Value arguments;
};

static LT_Slot_Descriptor EvalStackFrame_slots[] = {
    {"expression", offsetof(LT_EvalStackFrame, expression), &LT_SlotType_ReadonlyObject},
    {"environment", offsetof(LT_EvalStackFrame, environment), &LT_SlotType_ReadonlyObject},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static LT_Slot_Descriptor ApplyStackFrame_slots[] = {
    {"callable", offsetof(LT_ApplyStackFrame, callable), &LT_SlotType_ReadonlyObject},
    {"arguments", offsetof(LT_ApplyStackFrame, arguments), &LT_SlotType_ReadonlyObject},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static void StackFrame_debugPrintOn(LT_Value obj, FILE* stream){
    (void)obj;
    fputs("#<StackFrame>", stream);
}

static void EvalStackFrame_debugPrintOn(LT_Value obj, FILE* stream){
    LT_EvalStackFrame* frame = LT_EvalStackFrame_from_value(obj);

    fputs("#<EvalStackFrame expr=", stream);
    LT_Value_debugPrintOn(frame->expression, stream);
    fputs(" env=", stream);
    LT_Value_debugPrintOn((LT_Value)(uintptr_t)frame->environment, stream);
    fputc('>', stream);
}

static void ApplyStackFrame_debugPrintOn(LT_Value obj, FILE* stream){
    LT_ApplyStackFrame* frame = LT_ApplyStackFrame_from_value(obj);

    fputs("#<ApplyStackFrame callable=", stream);
    LT_Value_debugPrintOn(frame->callable, stream);
    fputs(" args=", stream);
    LT_Value_debugPrintOn(frame->arguments, stream);
    fputc('>', stream);
}

LT_DEFINE_CLASS(LT_StackFrameObject) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "StackFrame",
    .instance_size = sizeof(LT_StackFrameObject),
    .class_flags = LT_CLASS_FLAG_ABSTRACT,
    .debugPrintOn = StackFrame_debugPrintOn,
};

LT_DEFINE_CLASS(LT_EvalStackFrame) {
    .superclass = &LT_StackFrameObject_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "EvalStackFrame",
    .instance_size = sizeof(LT_EvalStackFrame),
    .debugPrintOn = EvalStackFrame_debugPrintOn,
    .slots = EvalStackFrame_slots,
};

LT_DEFINE_CLASS(LT_ApplyStackFrame) {
    .superclass = &LT_StackFrameObject_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "ApplyStackFrame",
    .instance_size = sizeof(LT_ApplyStackFrame),
    .debugPrintOn = ApplyStackFrame_debugPrintOn,
    .slots = ApplyStackFrame_slots,
};

LT_EvalStackFrame* LT_EvalStackFrame_new(
    LT_Value expression,
    LT_Environment* environment
){
    LT_EvalStackFrame* frame = LT_Class_ALLOC(LT_EvalStackFrame);
    frame->expression = expression;
    frame->environment = environment;
    return frame;
}

LT_Value LT_EvalStackFrame_expression(LT_EvalStackFrame* frame){
    return frame->expression;
}

LT_Environment* LT_EvalStackFrame_environment(LT_EvalStackFrame* frame){
    return frame->environment;
}

LT_ApplyStackFrame* LT_ApplyStackFrame_new(LT_Value callable, LT_Value arguments){
    LT_ApplyStackFrame* frame = LT_Class_ALLOC(LT_ApplyStackFrame);
    frame->callable = callable;
    frame->arguments = arguments;
    return frame;
}

LT_Value LT_ApplyStackFrame_callable(LT_ApplyStackFrame* frame){
    return frame->callable;
}

LT_Value LT_ApplyStackFrame_arguments(LT_ApplyStackFrame* frame){
    return frame->arguments;
}
