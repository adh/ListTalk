/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Condition.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/Class.h>

#include <stddef.h>
#include <stdarg.h>
#include <string.h>

struct LT_Condition_s {
    LT_Object base;
    LT_Value message;
    LT_Value args;
};

struct LT_Warning_s {
    LT_Condition base;
};

struct LT_Error_s {
    LT_Condition base;
};

struct LT_SubclassResponsibilityError_s {
    LT_Error base;
};

struct LT_SystemError_s {
    LT_Error base;
    LT_Value errno_value;
};

struct LT_ReaderError_s {
    LT_Error base;
    LT_Value line;
    LT_Value column;
    LT_Value nesting_depth;
};

struct LT_IncompleteInputSyntaxError_s {
    LT_ReaderError base;
};

static LT_Slot_Descriptor Condition_slots[] = {
    {"message", offsetof(LT_Condition, message), &LT_SlotType_ReadonlyObject},
    {"args", offsetof(LT_Condition, args), &LT_SlotType_ReadonlyObject},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static LT_Slot_Descriptor ReaderError_slots[] = {
    {
        "line",
        offsetof(LT_ReaderError, line),
        &LT_SlotType_ReadonlyObject
    },
    {
        "column",
        offsetof(LT_ReaderError, column),
        &LT_SlotType_ReadonlyObject
    },
    {
        "nesting-depth",
        offsetof(LT_ReaderError, nesting_depth),
        &LT_SlotType_ReadonlyObject
    },
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static LT_Slot_Descriptor SystemError_slots[] = {
    {"errno", offsetof(LT_SystemError, errno_value), &LT_SlotType_ReadonlyObject},
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

LT_DEFINE_CLASS(LT_Warning) {
    .superclass = &LT_Condition_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Warning",
    .instance_size = sizeof(LT_Warning),
    .debugPrintOn = Condition_debugPrintOn,
};

LT_DEFINE_CLASS(LT_Error) {
    .superclass = &LT_Condition_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Error",
    .instance_size = sizeof(LT_Error),
    .debugPrintOn = Condition_debugPrintOn,
};

LT_DEFINE_CLASS(LT_SubclassResponsibilityError) {
    .superclass = &LT_Error_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "SubclassResponsibilityError",
    .instance_size = sizeof(LT_SubclassResponsibilityError),
    .debugPrintOn = Condition_debugPrintOn,
};

LT_DEFINE_CLASS(LT_SystemError) {
    .superclass = &LT_Error_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "SystemError",
    .instance_size = sizeof(LT_SystemError),
    .debugPrintOn = Condition_debugPrintOn,
    .slots = SystemError_slots,
};

LT_DEFINE_CLASS(LT_ReaderError) {
    .superclass = &LT_Error_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "ReaderError",
    .instance_size = sizeof(LT_ReaderError),
    .debugPrintOn = Condition_debugPrintOn,
    .slots = ReaderError_slots,
};

LT_DEFINE_CLASS(LT_IncompleteInputSyntaxError) {
    .superclass = &LT_ReaderError_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "IncompleteInputSyntaxError",
    .instance_size = sizeof(LT_IncompleteInputSyntaxError),
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

LT_Value LT_SystemError_new(const char* message, int errnum, LT_Value args){
    char* full_message = LT_sprintf("%s: %s", message, strerror(errnum));
    LT_Value value = LT_Condition_new(&LT_SystemError_class, full_message, args);
    LT_SystemError* condition = (LT_SystemError*)LT_VALUE_POINTER_VALUE(value);

    condition->errno_value = LT_SmallInteger_new((int64_t)errnum);
    return value;
}

LT_Value LT_ReaderError_new(
    LT_Class* klass,
    const char* message,
    LT_Value args,
    LT_Value line,
    LT_Value column,
    LT_Value nesting_depth
){
    LT_Value value = LT_Condition_new(klass, message, args);
    LT_ReaderError* condition =
        (LT_ReaderError*)LT_VALUE_POINTER_VALUE(value);

    condition->line = line;
    condition->column = column;
    condition->nesting_depth = nesting_depth;
    return value;
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
    result = LT_Condition_vnew(&LT_Warning_class, message, args);
    va_end(args);
    return result;
}

LT_Value LT_Error_impl(const char* message, ...){
    LT_Value result;
    va_list args;

    va_start(args, message);
    result = LT_Condition_vnew(&LT_Error_class, message, args);
    va_end(args);
    return result;
}

LT_Value LT_SubclassResponsibilityError_impl(const char* message, ...){
    LT_Value result;
    va_list args;

    va_start(args, message);
    result = LT_Condition_vnew(
        &LT_SubclassResponsibilityError_class,
        message,
        args
    );
    va_end(args);
    return result;
}
