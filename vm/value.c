/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/vm/value.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/classes/Nil.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/classes/Primitive.h>

LT_Class LT_Float_class = {0};
LT_Class* const LT__Immediate_classes[64] = {
    [LT_VALUE_IMMEDIATE_TAG_NIL & 0x3f] = &LT_Nil_class,
    [LT_VALUE_IMMEDIATE_TAG_FIXNUM & 0x3f] = &LT_SmallInteger_class,
};
LT_Class* const LT__Pointer_classes[8] = {
    &LT_Class_class,
    &LT_Class_class,
    &LT_Pair_class,
    &LT_Symbol_class,
    &LT_Class_class,
    &LT_Primitive_class,
    &LT_Class_class,
    &LT_Class_class
};

void* LT_Class_alloc(LT_Class *klass)
{
    LT_Object* obj = GC_MALLOC(klass->instance_size);
    obj->klass = klass;
    return obj;
}
void* LT_Class_alloc_flexible(LT_Class *klass, size_t flex)
{
    LT_Object* obj = GC_MALLOC(klass->instance_size + flex);
    obj->klass = klass;
    return obj;
}
