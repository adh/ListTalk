/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/MessageDigest.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/DigestSHA256.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/utils/crypto/sha256.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>

#include <stdint.h>

struct LT_DigestSHA256_s {
    LT_Object base;
    LT_SHA256 digest;
    int finished;
};

LT_DEFINE_PRIMITIVE(
    digest_sha256_class_method_new,
    "DigestSHA256 class>>new",
    "(self)",
    "Return a new incremental SHA-256 message digest."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    if (self != (LT_Value)(uintptr_t)&LT_DigestSHA256_class){
        LT_error("new class method is only supported on DigestSHA256");
    }
    return (LT_Value)(uintptr_t)LT_DigestSHA256_new();
}

LT_DEFINE_PRIMITIVE(
    digest_sha256_class_method_digest,
    "DigestSHA256 class>>digest:",
    "(self bytes)",
    "Return SHA-256 digest of bytes as a bytevector."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_ByteVector* bytes;
    uint8_t digest[LT_SHA256_DIGEST_LENGTH];
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, bytes, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);

    if (self != (LT_Value)(uintptr_t)&LT_DigestSHA256_class){
        LT_error("digest: class method is only supported on DigestSHA256");
    }
    LT_SHA256_digest(
        LT_ByteVector_bytes(bytes),
        LT_ByteVector_length(bytes),
        digest
    );
    return (LT_Value)(uintptr_t)LT_ByteVector_new(digest, sizeof(digest));
}

LT_DEFINE_PRIMITIVE(
    digest_sha256_method_update,
    "DigestSHA256>>update:",
    "(self bytes)",
    "Update SHA-256 digest state with bytes and return receiver."
){
    LT_Value cursor = arguments;
    LT_DigestSHA256* digest;
    LT_ByteVector* bytes;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, digest, LT_DigestSHA256*, LT_DigestSHA256_from_value);
    LT_GENERIC_ARG(cursor, bytes, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);

    if (digest->finished){
        LT_error("DigestSHA256 digest is already finished");
    }
    LT_SHA256_update(
        &digest->digest,
        LT_ByteVector_bytes(bytes),
        LT_ByteVector_length(bytes)
    );
    return (LT_Value)(uintptr_t)digest;
}

LT_DEFINE_PRIMITIVE(
    digest_sha256_method_finish,
    "DigestSHA256>>finish",
    "(self)",
    "Finish SHA-256 digest computation and return digest bytes."
){
    LT_Value cursor = arguments;
    LT_DigestSHA256* digest;
    uint8_t digest_bytes[LT_SHA256_DIGEST_LENGTH];
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(cursor, digest, LT_DigestSHA256*, LT_DigestSHA256_from_value);
    LT_ARG_END(cursor);

    if (digest->finished){
        LT_error("DigestSHA256 digest is already finished");
    }
    LT_SHA256_finish(&digest->digest, digest_bytes);
    digest->finished = 1;
    return (LT_Value)(uintptr_t)LT_ByteVector_new(
        digest_bytes,
        sizeof(digest_bytes)
    );
}

static LT_Method_Descriptor DigestSHA256_methods[] = {
    {"update:", &digest_sha256_method_update},
    {"finish", &digest_sha256_method_finish},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor DigestSHA256_class_methods[] = {
    {"new", &digest_sha256_class_method_new},
    {"digest:", &digest_sha256_class_method_digest},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_DigestSHA256) {
    .superclass = &LT_MessageDigest_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "DigestSHA256",
    .documentation = "SHA-256 message digest.",
    .instance_size = sizeof(LT_DigestSHA256),
    .methods = DigestSHA256_methods,
    .class_methods = DigestSHA256_class_methods,
};

LT_DigestSHA256* LT_DigestSHA256_new(void){
    LT_DigestSHA256* digest = LT_Class_ALLOC(LT_DigestSHA256);

    LT_SHA256_init(&digest->digest);
    digest->finished = 0;
    return digest;
}
