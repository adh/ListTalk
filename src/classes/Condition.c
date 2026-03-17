/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Condition.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/Class.h>

#include <stddef.h>
#include <stdarg.h>

struct LT_Condition_s {
    LT_Object base;
    LT_Value message;
    LT_Value args;
};

struct LT_WarningCondition_s {
    LT_Condition base;
};

struct LT_ErrorCondition_s {
    LT_Condition base;
};

static LT_Slot_Descriptor Condition_slots[] = {
    {"message", offsetof(LT_Condition, message), &LT_SlotType_ReadonlyObject},
    {"args", offsetof(LT_Condition, args), &LT_SlotType_ReadonlyObject},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static void Condition_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Condition* condition = (LT_Condition*)LT_VALUE_POINTER_VALUE(obj);
    LT_Class* klass = LT_Value_class(obj);

    fputs("#<", stream);
    if (LT_Symbol_p(klass->name)){
        fputs(LT_Symbol_name(LT_Symbol_from_value(klass->name)), stream);
    } else {
        fputs("Condition", stream);
    }
    fputs(" message=", stream);
    LT_Value_debugPrintOn(condition->message, stream);
    fputs(" args=", stream);
    LT_Value_debugPrintOn(condition->args, stream);
    fputc('>', stream);
}

LT_DEFINE_CLASS(LT_Condition) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Condition",
    .instance_size = sizeof(LT_Condition),
    .debugPrintOn = Condition_debugPrintOn,
    .slots = Condition_slots,
};

LT_DEFINE_CLASS(LT_WarningCondition) {
    .superclass = &LT_Condition_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Warning",
    .instance_size = sizeof(LT_WarningCondition),
    .debugPrintOn = Condition_debugPrintOn,
};

LT_DEFINE_CLASS(LT_ErrorCondition) {
    .superclass = &LT_Condition_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Error",
    .instance_size = sizeof(LT_ErrorCondition),
    .debugPrintOn = Condition_debugPrintOn,
};

LT_Value LT_Condition_new(LT_Class* klass, const char* message, LT_Value args){
    LT_Condition* condition = (LT_Condition*)LT_Class_alloc(klass);

    condition->message = (LT_Value)(uintptr_t)LT_String_new_cstr((char*)message);
    condition->args = args;
    return (LT_Value)(uintptr_t)condition;
}

LT_Value LT_Condition_vnew(LT_Class* klass, const char* message, va_list args){
    LT_ListBuilder* builder = LT_ListBuilder_new();
    const char* key;

    while ((key = va_arg(args, const char*)) != NULL){
        LT_Value value = va_arg(args, LT_Value);
        LT_ListBuilder_append(builder, LT_Symbol_new((char*)key));
        LT_ListBuilder_append(builder, value);
    }

    return LT_Condition_new(
        klass,
        message,
        LT_ListBuilder_valueWithRest(builder, LT_NIL)
    );
}

LT_Value LT_Condition_impl(const char* message, ...){
    LT_Value result;
    va_list args;

    va_start(args, message);
    result = LT_Condition_vnew(&LT_Condition_class, message, args);
    va_end(args);
    return result;
}

LT_Value LT_Warning_impl(const char* message, ...){
    LT_Value result;
    va_list args;

    va_start(args, message);
    result = LT_Condition_vnew(&LT_WarningCondition_class, message, args);
    va_end(args);
    return result;
}

LT_Value LT_Error_impl(const char* message, ...){
    LT_Value result;
    va_list args;

    va_start(args, message);
    result = LT_Condition_vnew(&LT_ErrorCondition_class, message, args);
    va_end(args);
    return result;
}
