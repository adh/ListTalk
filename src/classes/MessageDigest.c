/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/MessageDigest.h>
#include <ListTalk/classes/Object.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/macros/method_macros.h>
#include <ListTalk/vm/Class.h>

struct LT_MessageDigest_s {
    LT_Object base;
};

LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_1(
    message_digest_method_update,
    "MessageDigest>>update:",
    bytes,
    "Update digest state with bytes and return receiver."
)

LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    message_digest_method_finish,
    "MessageDigest>>finish",
    "Finish digest computation and return digest bytes."
)

static LT_Method_Descriptor MessageDigest_methods[] = {
    {"update:", &message_digest_method_update},
    {"finish", &message_digest_method_finish},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_MessageDigest) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "MessageDigest",
    .documentation = "Abstract root for incremental message digests.",
    .instance_size = sizeof(LT_MessageDigest),
    .class_flags = LT_CLASS_FLAG_ABSTRACT,
    .methods = MessageDigest_methods,
};
