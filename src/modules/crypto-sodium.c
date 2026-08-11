/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>

#include <sodium.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

LT_DECLARE_CLASS(LT_Ristretto255Element);

struct LT_Ristretto255Element_s {
    LT_Object base;
    uint8_t bytes[crypto_core_ristretto255_BYTES];
};

static void bind_sodium_primitive(LT_Environment* environment,
                                  LT_Package* package,
                                  LT_Primitive* primitive){
    LT_Environment_bind(
        environment,
        LT_Symbol_new_in(package, primitive->name),
        LT_Primitive_from_static(primitive),
        LT_ENV_BINDING_FLAG_CONSTANT
    );
}

static void bind_sodium_constant(LT_Environment* environment,
                                 LT_Package* package,
                                 const char* name,
                                 LT_Value value){
    LT_Environment_bind(
        environment,
        LT_Symbol_new_in(package, (char*)name),
        value,
        LT_ENV_BINDING_FLAG_CONSTANT
    );
}

static LT_Value sodium_size_value(size_t value){
    return LT_Number_smallinteger_from_size(
        value,
        "libsodium constant does not fit fixnum"
    );
}

static LT_ByteVector* sodium_bytevector_of_size(size_t length){
    uint8_t* bytes = GC_MALLOC_ATOMIC(length == 0 ? 1 : length);

    return LT_ByteVector_new(bytes, length);
}

static uint8_t* sodium_output_bytes(size_t length){
    return GC_MALLOC_ATOMIC(length == 0 ? 1 : length);
}

static size_t checked_added_length(size_t length,
                                   size_t extra,
                                   const char* message){
    if (length > SIZE_MAX - extra){
        LT_error(message);
    }
    return length + extra;
}

static void expect_bytevector_length(LT_ByteVector* bytevector,
                                     size_t expected,
                                     const char* message){
    if (LT_ByteVector_length(bytevector) != expected){
        LT_error(message);
    }
}

static size_t output_length_from_value(LT_Value value,
                                       size_t min,
                                       size_t max,
                                       const char* message){
    size_t length = LT_Number_nonnegative_size_from_integer(
        value,
        message,
        message
    );

    if (length < min || length > max){
        LT_error(message);
    }
    return length;
}

static LT_Ristretto255Element* ristretto255_element_new(const uint8_t* bytes){
    LT_Ristretto255Element* element = LT_Class_ALLOC(LT_Ristretto255Element);

    memcpy(element->bytes, bytes, crypto_core_ristretto255_BYTES);
    return element;
}

static LT_Ristretto255Element* ristretto255_element_from_bytevector(
    LT_ByteVector* bytevector
){
    if (LT_ByteVector_length(bytevector) != crypto_core_ristretto255_BYTES){
        LT_error("Ristretto255 element bytevector must be exactly 32 bytes");
    }
    if (crypto_core_ristretto255_is_valid_point(
            LT_ByteVector_bytes(bytevector)
        ) != 1){
        LT_error("Invalid Ristretto255 element encoding");
    }
    return ristretto255_element_new(LT_ByteVector_bytes(bytevector));
}

static void expect_ristretto255_hash(LT_ByteVector* hash){
    expect_bytevector_length(
        hash,
        crypto_core_ristretto255_HASHBYTES,
        "Ristretto255 hash input must be exactly 64 bytes"
    );
}

static void expect_ristretto255_scalar(LT_ByteVector* scalar){
    expect_bytevector_length(
        scalar,
        crypto_core_ristretto255_SCALARBYTES,
        "Ristretto255 scalar must be exactly 32 bytes"
    );
}

static size_t ristretto255_element_hash(LT_Value value){
    LT_Ristretto255Element* element = LT_Ristretto255Element_from_value(value);
    uint32_t hash = UINT32_C(0x811c9dc5);
    size_t i;

    for (i = 0; i < crypto_core_ristretto255_BYTES; i++){
        hash += (uint32_t)element->bytes[i];
        hash *= UINT32_C(0x01000193);
    }
    return (size_t)hash;
}

static int ristretto255_element_equal_p(LT_Value left, LT_Value right){
    LT_Ristretto255Element* left_element;
    LT_Ristretto255Element* right_element;

    if (!LT_Ristretto255Element_p(right)){
        return 0;
    }
    left_element = LT_Ristretto255Element_from_value(left);
    right_element = LT_Ristretto255Element_from_value(right);
    return sodium_memcmp(
        left_element->bytes,
        right_element->bytes,
        crypto_core_ristretto255_BYTES
    ) == 0;
}

static void ristretto255_element_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Ristretto255Element* element = LT_Ristretto255Element_from_value(obj);
    char hex[crypto_core_ristretto255_BYTES * 2 + 1];

    sodium_bin2hex(
        hex,
        sizeof(hex),
        element->bytes,
        crypto_core_ristretto255_BYTES
    );
    fprintf(stream, "#<Ristretto255Element %s>", hex);
}

LT_DEFINE_PRIMITIVE(
    ristretto255_element_class_method_random,
    "Ristretto255Element class>>random",
    "(self)",
    "Return a random Ristretto255 element."
){
    LT_Value cursor = arguments;
    LT_Value self;
    uint8_t bytes[crypto_core_ristretto255_BYTES];

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    if (self != LT_STATIC_CLASS(LT_Ristretto255Element)){
        LT_error("random class method is only supported on Ristretto255Element");
    }
    crypto_core_ristretto255_random(bytes);
    return (LT_Value)(uintptr_t)ristretto255_element_new(bytes);
}

LT_DEFINE_PRIMITIVE(
    ristretto255_element_class_method_from_hash,
    "Ristretto255Element class>>fromHash:",
    "(self hash)",
    "Return a Ristretto255 element mapped from a 64-byte hash."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_ByteVector* hash;
    uint8_t bytes[crypto_core_ristretto255_BYTES];

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, hash, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    if (self != LT_STATIC_CLASS(LT_Ristretto255Element)){
        LT_error("fromHash: class method is only supported on Ristretto255Element");
    }
    expect_ristretto255_hash(hash);
    crypto_core_ristretto255_from_hash(bytes, LT_ByteVector_bytes(hash));
    return (LT_Value)(uintptr_t)ristretto255_element_new(bytes);
}

LT_DEFINE_PRIMITIVE(
    ristretto255_element_class_method_from_bytevector,
    "Ristretto255Element class>>fromByteVector:",
    "(self bytevector)",
    "Return a Ristretto255 element from canonical bytes."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_ByteVector* bytevector;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, bytevector, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    if (self != LT_STATIC_CLASS(LT_Ristretto255Element)){
        LT_error("fromByteVector: class method is only supported on Ristretto255Element");
    }
    return (LT_Value)(uintptr_t)ristretto255_element_from_bytevector(bytevector);
}

LT_DEFINE_PRIMITIVE(
    ristretto255_element_class_method_valid_bytevector_p,
    "Ristretto255Element class>>validByteVector?:",
    "(self bytevector)",
    "Return true when bytevector is a canonical Ristretto255 element encoding."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_ByteVector* bytevector;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, bytevector, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    if (self != LT_STATIC_CLASS(LT_Ristretto255Element)){
        LT_error("validByteVector?: class method is only supported on Ristretto255Element");
    }
    if (LT_ByteVector_length(bytevector) != crypto_core_ristretto255_BYTES){
        return LT_FALSE;
    }
    return crypto_core_ristretto255_is_valid_point(
        LT_ByteVector_bytes(bytevector)
    ) == 1 ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    ristretto255_element_class_method_from_base_scalar,
    "Ristretto255Element class>>fromBaseScalar:",
    "(self scalar)",
    "Return scalar times the Ristretto255 base point."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_ByteVector* scalar;
    uint8_t bytes[crypto_core_ristretto255_BYTES];

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, scalar, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    if (self != LT_STATIC_CLASS(LT_Ristretto255Element)){
        LT_error("fromBaseScalar: class method is only supported on Ristretto255Element");
    }
    expect_ristretto255_scalar(scalar);
    crypto_scalarmult_ristretto255_base(bytes, LT_ByteVector_bytes(scalar));
    return (LT_Value)(uintptr_t)ristretto255_element_new(bytes);
}

LT_DEFINE_PRIMITIVE(
    ristretto255_element_method_as_bytevector,
    "Ristretto255Element>>asByteVector",
    "(self)",
    "Return receiver canonical bytes as a bytevector."
){
    LT_Value cursor = arguments;
    LT_Ristretto255Element* self;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_Ristretto255Element*, LT_Ristretto255Element_from_value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_ByteVector_new(
        self->bytes,
        crypto_core_ristretto255_BYTES
    );
}

static LT_Value ristretto255_element_add2(LT_Ristretto255Element* left,
                                          LT_Ristretto255Element* right){
    uint8_t bytes[crypto_core_ristretto255_BYTES];

    if (crypto_core_ristretto255_add(bytes, left->bytes, right->bytes) != 0){
        LT_error("Ristretto255 addition failed");
    }
    return (LT_Value)(uintptr_t)ristretto255_element_new(bytes);
}

static LT_Value ristretto255_element_subtract2(LT_Ristretto255Element* left,
                                               LT_Ristretto255Element* right){
    uint8_t bytes[crypto_core_ristretto255_BYTES];

    if (crypto_core_ristretto255_sub(bytes, left->bytes, right->bytes) != 0){
        LT_error("Ristretto255 subtraction failed");
    }
    return (LT_Value)(uintptr_t)ristretto255_element_new(bytes);
}

LT_DEFINE_PRIMITIVE(
    ristretto255_element_method_add,
    "Ristretto255Element>>add:",
    "(self other)",
    "Return the sum of two Ristretto255 elements."
){
    LT_Value cursor = arguments;
    LT_Ristretto255Element* self;
    LT_Ristretto255Element* other;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_Ristretto255Element*, LT_Ristretto255Element_from_value);
    LT_GENERIC_ARG(cursor, other, LT_Ristretto255Element*, LT_Ristretto255Element_from_value);
    LT_ARG_END(cursor);
    return ristretto255_element_add2(self, other);
}

LT_DEFINE_PRIMITIVE(
    ristretto255_element_method_plus,
    "Ristretto255Element>>+",
    "(self other)",
    "Return the sum of two Ristretto255 elements."
){
    LT_Value cursor = arguments;
    LT_Ristretto255Element* self;
    LT_Ristretto255Element* other;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_Ristretto255Element*, LT_Ristretto255Element_from_value);
    LT_GENERIC_ARG(cursor, other, LT_Ristretto255Element*, LT_Ristretto255Element_from_value);
    LT_ARG_END(cursor);
    return ristretto255_element_add2(self, other);
}

LT_DEFINE_PRIMITIVE(
    ristretto255_element_method_subtract,
    "Ristretto255Element>>subtract:",
    "(self other)",
    "Return the difference of two Ristretto255 elements."
){
    LT_Value cursor = arguments;
    LT_Ristretto255Element* self;
    LT_Ristretto255Element* other;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_Ristretto255Element*, LT_Ristretto255Element_from_value);
    LT_GENERIC_ARG(cursor, other, LT_Ristretto255Element*, LT_Ristretto255Element_from_value);
    LT_ARG_END(cursor);
    return ristretto255_element_subtract2(self, other);
}

LT_DEFINE_PRIMITIVE(
    ristretto255_element_method_minus,
    "Ristretto255Element>>-",
    "(self other)",
    "Return the difference of two Ristretto255 elements."
){
    LT_Value cursor = arguments;
    LT_Ristretto255Element* self;
    LT_Ristretto255Element* other;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_Ristretto255Element*, LT_Ristretto255Element_from_value);
    LT_GENERIC_ARG(cursor, other, LT_Ristretto255Element*, LT_Ristretto255Element_from_value);
    LT_ARG_END(cursor);
    return ristretto255_element_subtract2(self, other);
}

LT_DEFINE_PRIMITIVE(
    ristretto255_element_method_negate,
    "Ristretto255Element>>negate",
    "(self)",
    "Return the additive inverse of a Ristretto255 element."
){
    LT_Value cursor = arguments;
    LT_Ristretto255Element* self;
    uint8_t bytes[crypto_core_ristretto255_BYTES];
    static const uint8_t identity[crypto_core_ristretto255_BYTES] = {0};

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_Ristretto255Element*, LT_Ristretto255Element_from_value);
    LT_ARG_END(cursor);
    if (crypto_core_ristretto255_sub(bytes, identity, self->bytes) != 0){
        LT_error("Ristretto255 negation failed");
    }
    return (LT_Value)(uintptr_t)ristretto255_element_new(bytes);
}

LT_DEFINE_PRIMITIVE(
    ristretto255_element_method_scalar_multiply_by,
    "Ristretto255Element>>scalarMultiplyBy:",
    "(self scalar)",
    "Return scalar times receiver."
){
    LT_Value cursor = arguments;
    LT_Ristretto255Element* self;
    LT_ByteVector* scalar;
    uint8_t bytes[crypto_core_ristretto255_BYTES];

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, self, LT_Ristretto255Element*, LT_Ristretto255Element_from_value);
    LT_GENERIC_ARG(cursor, scalar, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    expect_ristretto255_scalar(scalar);
    if (crypto_scalarmult_ristretto255(
            bytes,
            LT_ByteVector_bytes(scalar),
            self->bytes
        ) != 0){
        LT_error("Ristretto255 scalar multiplication failed");
    }
    return (LT_Value)(uintptr_t)ristretto255_element_new(bytes);
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_ristretto255_scalar_random,
    "ristretto255-scalar-random",
    "()",
    "Return a random Ristretto255 scalar."
){
    uint8_t* scalar;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_ARG_END(arguments);
    scalar = sodium_output_bytes(crypto_core_ristretto255_SCALARBYTES);
    crypto_core_ristretto255_scalar_random(scalar);
    return (LT_Value)(uintptr_t)LT_ByteVector_new(
        scalar,
        crypto_core_ristretto255_SCALARBYTES
    );
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_ristretto255_scalar_reduce,
    "ristretto255-scalar-reduce",
    "(bytes)",
    "Reduce 64 bytes to a canonical Ristretto255 scalar."
){
    LT_Value cursor = arguments;
    LT_ByteVector* bytes;
    uint8_t* scalar;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, bytes, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    expect_bytevector_length(
        bytes,
        crypto_core_ristretto255_NONREDUCEDSCALARBYTES,
        "Ristretto255 non-reduced scalar must be exactly 64 bytes"
    );
    scalar = sodium_output_bytes(crypto_core_ristretto255_SCALARBYTES);
    crypto_core_ristretto255_scalar_reduce(scalar, LT_ByteVector_bytes(bytes));
    return (LT_Value)(uintptr_t)LT_ByteVector_new(
        scalar,
        crypto_core_ristretto255_SCALARBYTES
    );
}

static LT_Method_Descriptor Ristretto255Element_methods[] = {
    {"asByteVector", &ristretto255_element_method_as_bytevector},
    {"add:", &ristretto255_element_method_add},
    {"+", &ristretto255_element_method_plus},
    {"subtract:", &ristretto255_element_method_subtract},
    {"-", &ristretto255_element_method_minus},
    {"negate", &ristretto255_element_method_negate},
    {"scalarMultiplyBy:", &ristretto255_element_method_scalar_multiply_by},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor Ristretto255Element_class_methods[] = {
    {"random", &ristretto255_element_class_method_random},
    {"fromHash:", &ristretto255_element_class_method_from_hash},
    {"fromByteVector:", &ristretto255_element_class_method_from_bytevector},
    {"validByteVector?:", &ristretto255_element_class_method_valid_bytevector_p},
    {"fromBaseScalar:", &ristretto255_element_class_method_from_base_scalar},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Ristretto255Element) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Ristretto255Element",
    .package = "ListTalk:crypto-sodium",
    .documentation = "Immutable Ristretto255 group element.",
    .instance_size = sizeof(LT_Ristretto255Element),
    .class_flags = LT_CLASS_FLAG_IMMUTABLE | LT_CLASS_FLAG_SCALAR,
    .hash = ristretto255_element_hash,
    .equal_p = ristretto255_element_equal_p,
    .debugPrintOn = ristretto255_element_debugPrintOn,
    .methods = Ristretto255Element_methods,
    .class_methods = Ristretto255Element_class_methods,
};

LT_DEFINE_PRIMITIVE(
    primitive_sodium_randombytes,
    "randombytes",
    "(length)",
    "Return length cryptographically random bytes."
){
    LT_Value cursor = arguments;
    LT_Value length_value;
    LT_ByteVector* output;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_OBJECT_ARG(cursor, length_value);
    LT_ARG_END(cursor);
    output = sodium_bytevector_of_size(LT_Number_nonnegative_size_from_integer(
        length_value,
        "Random byte count out of range",
        "Random byte count out of range"
    ));
    randombytes_buf((void*)LT_ByteVector_bytes(output), LT_ByteVector_length(output));
    return (LT_Value)(uintptr_t)output;
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_memzero_bang,
    "memzero!",
    "(bytes)",
    "Overwrite a bytevector with zero bytes."
){
    LT_Value cursor = arguments;
    LT_ByteVector* bytes;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, bytes, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    sodium_memzero((void*)LT_ByteVector_bytes(bytes), LT_ByteVector_length(bytes));
    return (LT_Value)(uintptr_t)bytes;
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_memcmp,
    "memcmp",
    "(left right)",
    "Return true when bytevectors are equal using libsodium's constant-time comparison."
){
    LT_Value cursor = arguments;
    LT_ByteVector* left;
    LT_ByteVector* right;
    size_t length;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, left, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, right, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);

    length = LT_ByteVector_length(left);
    if (length != LT_ByteVector_length(right)){
        return LT_FALSE;
    }
    return sodium_memcmp(
        LT_ByteVector_bytes(left),
        LT_ByteVector_bytes(right),
        length
    ) == 0 ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_generichash,
    "generichash",
    "(message :optional key output-length)",
    "Hash a bytevector with optional key and output length."
){
    LT_Value cursor = arguments;
    LT_ByteVector* message;
    LT_Value optional = LT_FALSE;
    LT_ByteVector* key = NULL;
    size_t output_length = crypto_generichash_BYTES;
    uint8_t* output;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, message, LT_ByteVector*, LT_ByteVector_from_value);
    if (cursor != LT_NIL){
        LT_OBJECT_ARG(cursor, optional);
        if (LT_Value_is_fixnum(optional)){
            output_length = output_length_from_value(
                optional,
                crypto_generichash_BYTES_MIN,
                crypto_generichash_BYTES_MAX,
                "Generic hash output length out of range"
            );
        } else if (optional != LT_FALSE && optional != LT_NIL){
            key = LT_ByteVector_from_value(optional);
            output_length = crypto_generichash_BYTES;
        }
    }
    if (cursor != LT_NIL){
        LT_OBJECT_ARG(cursor, optional);
        output_length = output_length_from_value(
            optional,
            crypto_generichash_BYTES_MIN,
            crypto_generichash_BYTES_MAX,
            "Generic hash output length out of range"
        );
    }
    LT_ARG_END(cursor);

    if (key != NULL){
        size_t key_length = LT_ByteVector_length(key);

        if (key_length < crypto_generichash_KEYBYTES_MIN
            || key_length > crypto_generichash_KEYBYTES_MAX){
            LT_error("Generic hash key length out of range");
        }
    }
    output = sodium_output_bytes(output_length);
    if (crypto_generichash(
            output,
            output_length,
            LT_ByteVector_bytes(message),
            LT_ByteVector_length(message),
            key == NULL ? NULL : LT_ByteVector_bytes(key),
            key == NULL ? 0 : LT_ByteVector_length(key)
        ) != 0){
        LT_error("libsodium generic hash failed");
    }
    return (LT_Value)(uintptr_t)LT_ByteVector_new(output, output_length);
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_generichash_keygen,
    "generichash-keygen",
    "()",
    "Return a random crypto_generichash key."
){
    uint8_t* key;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_ARG_END(arguments);
    key = sodium_output_bytes(crypto_generichash_KEYBYTES);
    crypto_generichash_keygen(key);
    return (LT_Value)(uintptr_t)LT_ByteVector_new(
        key,
        crypto_generichash_KEYBYTES
    );
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_shorthash,
    "shorthash",
    "(message key)",
    "Hash a short bytevector using SipHash-2-4."
){
    LT_Value cursor = arguments;
    LT_ByteVector* message;
    LT_ByteVector* key;
    uint8_t* output;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, message, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, key, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    expect_bytevector_length(key, crypto_shorthash_KEYBYTES, "Short hash key length out of range");

    output = sodium_output_bytes(crypto_shorthash_BYTES);
    crypto_shorthash(output, LT_ByteVector_bytes(message), LT_ByteVector_length(message), LT_ByteVector_bytes(key));
    return (LT_Value)(uintptr_t)LT_ByteVector_new(output, crypto_shorthash_BYTES);
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_shorthash_keygen,
    "shorthash-keygen",
    "()",
    "Return a random crypto_shorthash key."
){
    uint8_t* key;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_ARG_END(arguments);
    key = sodium_output_bytes(crypto_shorthash_KEYBYTES);
    crypto_shorthash_keygen(key);
    return (LT_Value)(uintptr_t)LT_ByteVector_new(key, crypto_shorthash_KEYBYTES);
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_auth,
    "auth",
    "(message key)",
    "Return a message authentication code."
){
    LT_Value cursor = arguments;
    LT_ByteVector* message;
    LT_ByteVector* key;
    uint8_t* output;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, message, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, key, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    expect_bytevector_length(key, crypto_auth_KEYBYTES, "Auth key length out of range");

    output = sodium_output_bytes(crypto_auth_BYTES);
    crypto_auth(output, LT_ByteVector_bytes(message), LT_ByteVector_length(message), LT_ByteVector_bytes(key));
    return (LT_Value)(uintptr_t)LT_ByteVector_new(output, crypto_auth_BYTES);
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_auth_verify,
    "auth-verify",
    "(mac message key)",
    "Return true when a message authentication code is valid."
){
    LT_Value cursor = arguments;
    LT_ByteVector* mac;
    LT_ByteVector* message;
    LT_ByteVector* key;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, mac, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, message, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, key, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    expect_bytevector_length(mac, crypto_auth_BYTES, "Auth MAC length out of range");
    expect_bytevector_length(key, crypto_auth_KEYBYTES, "Auth key length out of range");

    return crypto_auth_verify(
        LT_ByteVector_bytes(mac),
        LT_ByteVector_bytes(message),
        LT_ByteVector_length(message),
        LT_ByteVector_bytes(key)
    ) == 0
        ? LT_TRUE
        : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_auth_keygen,
    "auth-keygen",
    "()",
    "Return a random crypto_auth key."
){
    uint8_t* key;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_ARG_END(arguments);
    key = sodium_output_bytes(crypto_auth_KEYBYTES);
    crypto_auth_keygen(key);
    return (LT_Value)(uintptr_t)LT_ByteVector_new(key, crypto_auth_KEYBYTES);
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_secretbox_keygen,
    "secretbox-keygen",
    "()",
    "Return a random crypto_secretbox key."
){
    uint8_t* key;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_ARG_END(arguments);
    key = sodium_output_bytes(crypto_secretbox_KEYBYTES);
    crypto_secretbox_keygen(key);
    return (LT_Value)(uintptr_t)LT_ByteVector_new(key, crypto_secretbox_KEYBYTES);
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_secretbox_nonce,
    "secretbox-nonce",
    "()",
    "Return a random crypto_secretbox nonce."
){
    uint8_t* nonce;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_ARG_END(arguments);
    nonce = sodium_output_bytes(crypto_secretbox_NONCEBYTES);
    randombytes_buf(nonce, crypto_secretbox_NONCEBYTES);
    return (LT_Value)(uintptr_t)LT_ByteVector_new(nonce, crypto_secretbox_NONCEBYTES);
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_secretbox_easy,
    "secretbox-easy",
    "(message nonce key)",
    "Encrypt and authenticate a bytevector with crypto_secretbox_easy."
){
    LT_Value cursor = arguments;
    LT_ByteVector* message;
    LT_ByteVector* nonce;
    LT_ByteVector* key;
    size_t message_length;
    uint8_t* ciphertext;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, message, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, nonce, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, key, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    expect_bytevector_length(nonce, crypto_secretbox_NONCEBYTES, "Secretbox nonce length out of range");
    expect_bytevector_length(key, crypto_secretbox_KEYBYTES, "Secretbox key length out of range");

    message_length = LT_ByteVector_length(message);
    ciphertext = sodium_output_bytes(checked_added_length(
        message_length,
        crypto_secretbox_MACBYTES,
        "Secretbox ciphertext length out of range"
    ));
    crypto_secretbox_easy(
        ciphertext,
        LT_ByteVector_bytes(message),
        message_length,
        LT_ByteVector_bytes(nonce),
        LT_ByteVector_bytes(key)
    );
    return (LT_Value)(uintptr_t)LT_ByteVector_new(
        ciphertext,
        checked_added_length(
            message_length,
            crypto_secretbox_MACBYTES,
            "Secretbox ciphertext length out of range"
        )
    );
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_secretbox_open_easy,
    "secretbox-open-easy",
    "(ciphertext nonce key)",
    "Decrypt and verify crypto_secretbox_easy output, or return false."
){
    LT_Value cursor = arguments;
    LT_ByteVector* ciphertext;
    LT_ByteVector* nonce;
    LT_ByteVector* key;
    size_t ciphertext_length;
    size_t message_length;
    uint8_t* message;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, ciphertext, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, nonce, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, key, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    expect_bytevector_length(nonce, crypto_secretbox_NONCEBYTES, "Secretbox nonce length out of range");
    expect_bytevector_length(key, crypto_secretbox_KEYBYTES, "Secretbox key length out of range");

    ciphertext_length = LT_ByteVector_length(ciphertext);
    if (ciphertext_length < crypto_secretbox_MACBYTES){
        return LT_FALSE;
    }
    message_length = ciphertext_length - crypto_secretbox_MACBYTES;
    message = sodium_output_bytes(message_length);
    if (crypto_secretbox_open_easy(
            message,
            LT_ByteVector_bytes(ciphertext),
            ciphertext_length,
            LT_ByteVector_bytes(nonce),
            LT_ByteVector_bytes(key)
        ) != 0){
        return LT_FALSE;
    }
    return (LT_Value)(uintptr_t)LT_ByteVector_new(message, message_length);
}

static LT_Value sodium_keypair_value(const uint8_t* public_key,
                                     size_t public_key_length,
                                     const uint8_t* secret_key,
                                     size_t secret_key_length){
    return LT_listn(
        2,
        (LT_Value)(uintptr_t)LT_ByteVector_new(public_key, public_key_length),
        (LT_Value)(uintptr_t)LT_ByteVector_new(secret_key, secret_key_length)
    );
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_box_keypair,
    "box-keypair",
    "()",
    "Return a list of public and secret keys for crypto_box."
){
    uint8_t public_key[crypto_box_PUBLICKEYBYTES];
    uint8_t secret_key[crypto_box_SECRETKEYBYTES];

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_ARG_END(arguments);
    crypto_box_keypair(public_key, secret_key);
    return sodium_keypair_value(
        public_key,
        crypto_box_PUBLICKEYBYTES,
        secret_key,
        crypto_box_SECRETKEYBYTES
    );
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_box_seed_keypair,
    "box-seed-keypair",
    "(seed)",
    "Return a deterministic crypto_box keypair from seed."
){
    LT_Value cursor = arguments;
    LT_ByteVector* seed;
    uint8_t public_key[crypto_box_PUBLICKEYBYTES];
    uint8_t secret_key[crypto_box_SECRETKEYBYTES];

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, seed, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    expect_bytevector_length(seed, crypto_box_SEEDBYTES, "Box seed length out of range");
    crypto_box_seed_keypair(public_key, secret_key, LT_ByteVector_bytes(seed));
    return sodium_keypair_value(
        public_key,
        crypto_box_PUBLICKEYBYTES,
        secret_key,
        crypto_box_SECRETKEYBYTES
    );
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_box_nonce,
    "box-nonce",
    "()",
    "Return a random crypto_box nonce."
){
    uint8_t* nonce;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_ARG_END(arguments);
    nonce = sodium_output_bytes(crypto_box_NONCEBYTES);
    randombytes_buf(nonce, crypto_box_NONCEBYTES);
    return (LT_Value)(uintptr_t)LT_ByteVector_new(nonce, crypto_box_NONCEBYTES);
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_box_easy,
    "box-easy",
    "(message nonce recipient-public-key sender-secret-key)",
    "Encrypt and authenticate a bytevector with crypto_box_easy."
){
    LT_Value cursor = arguments;
    LT_ByteVector* message;
    LT_ByteVector* nonce;
    LT_ByteVector* recipient_public_key;
    LT_ByteVector* sender_secret_key;
    size_t message_length;
    uint8_t* ciphertext;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, message, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, nonce, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, recipient_public_key, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, sender_secret_key, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    expect_bytevector_length(nonce, crypto_box_NONCEBYTES, "Box nonce length out of range");
    expect_bytevector_length(recipient_public_key, crypto_box_PUBLICKEYBYTES, "Box public key length out of range");
    expect_bytevector_length(sender_secret_key, crypto_box_SECRETKEYBYTES, "Box secret key length out of range");

    message_length = LT_ByteVector_length(message);
    ciphertext = sodium_output_bytes(checked_added_length(
        message_length,
        crypto_box_MACBYTES,
        "Box ciphertext length out of range"
    ));
    if (crypto_box_easy(
            ciphertext,
            LT_ByteVector_bytes(message),
            message_length,
            LT_ByteVector_bytes(nonce),
            LT_ByteVector_bytes(recipient_public_key),
            LT_ByteVector_bytes(sender_secret_key)
        ) != 0){
        LT_error("libsodium box encryption failed");
    }
    return (LT_Value)(uintptr_t)LT_ByteVector_new(
        ciphertext,
        checked_added_length(
            message_length,
            crypto_box_MACBYTES,
            "Box ciphertext length out of range"
        )
    );
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_box_open_easy,
    "box-open-easy",
    "(ciphertext nonce sender-public-key recipient-secret-key)",
    "Decrypt and verify crypto_box_easy output, or return false."
){
    LT_Value cursor = arguments;
    LT_ByteVector* ciphertext;
    LT_ByteVector* nonce;
    LT_ByteVector* sender_public_key;
    LT_ByteVector* recipient_secret_key;
    size_t ciphertext_length;
    size_t message_length;
    uint8_t* message;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, ciphertext, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, nonce, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, sender_public_key, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, recipient_secret_key, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    expect_bytevector_length(nonce, crypto_box_NONCEBYTES, "Box nonce length out of range");
    expect_bytevector_length(sender_public_key, crypto_box_PUBLICKEYBYTES, "Box public key length out of range");
    expect_bytevector_length(recipient_secret_key, crypto_box_SECRETKEYBYTES, "Box secret key length out of range");

    ciphertext_length = LT_ByteVector_length(ciphertext);
    if (ciphertext_length < crypto_box_MACBYTES){
        return LT_FALSE;
    }
    message_length = ciphertext_length - crypto_box_MACBYTES;
    message = sodium_output_bytes(message_length);
    if (crypto_box_open_easy(
            message,
            LT_ByteVector_bytes(ciphertext),
            ciphertext_length,
            LT_ByteVector_bytes(nonce),
            LT_ByteVector_bytes(sender_public_key),
            LT_ByteVector_bytes(recipient_secret_key)
        ) != 0){
        return LT_FALSE;
    }
    return (LT_Value)(uintptr_t)LT_ByteVector_new(message, message_length);
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_box_seal,
    "box-seal",
    "(message recipient-public-key)",
    "Encrypt a bytevector anonymously with crypto_box_seal."
){
    LT_Value cursor = arguments;
    LT_ByteVector* message;
    LT_ByteVector* recipient_public_key;
    size_t message_length;
    uint8_t* ciphertext;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, message, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, recipient_public_key, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    expect_bytevector_length(recipient_public_key, crypto_box_PUBLICKEYBYTES, "Box public key length out of range");

    message_length = LT_ByteVector_length(message);
    ciphertext = sodium_output_bytes(checked_added_length(
        message_length,
        crypto_box_SEALBYTES,
        "Box sealed ciphertext length out of range"
    ));
    crypto_box_seal(
        ciphertext,
        LT_ByteVector_bytes(message),
        message_length,
        LT_ByteVector_bytes(recipient_public_key)
    );
    return (LT_Value)(uintptr_t)LT_ByteVector_new(
        ciphertext,
        checked_added_length(
            message_length,
            crypto_box_SEALBYTES,
            "Box sealed ciphertext length out of range"
        )
    );
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_box_seal_open,
    "box-seal-open",
    "(ciphertext recipient-public-key recipient-secret-key)",
    "Decrypt and verify crypto_box_seal output, or return false."
){
    LT_Value cursor = arguments;
    LT_ByteVector* ciphertext;
    LT_ByteVector* recipient_public_key;
    LT_ByteVector* recipient_secret_key;
    size_t ciphertext_length;
    size_t message_length;
    uint8_t* message;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, ciphertext, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, recipient_public_key, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, recipient_secret_key, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    expect_bytevector_length(recipient_public_key, crypto_box_PUBLICKEYBYTES, "Box public key length out of range");
    expect_bytevector_length(recipient_secret_key, crypto_box_SECRETKEYBYTES, "Box secret key length out of range");

    ciphertext_length = LT_ByteVector_length(ciphertext);
    if (ciphertext_length < crypto_box_SEALBYTES){
        return LT_FALSE;
    }
    message_length = ciphertext_length - crypto_box_SEALBYTES;
    message = sodium_output_bytes(message_length);
    if (crypto_box_seal_open(
            message,
            LT_ByteVector_bytes(ciphertext),
            ciphertext_length,
            LT_ByteVector_bytes(recipient_public_key),
            LT_ByteVector_bytes(recipient_secret_key)
        ) != 0){
        return LT_FALSE;
    }
    return (LT_Value)(uintptr_t)LT_ByteVector_new(message, message_length);
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_sign_keypair,
    "sign-keypair",
    "()",
    "Return a list of public and secret keys for crypto_sign."
){
    uint8_t public_key[crypto_sign_PUBLICKEYBYTES];
    uint8_t secret_key[crypto_sign_SECRETKEYBYTES];

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_ARG_END(arguments);
    crypto_sign_keypair(public_key, secret_key);
    return sodium_keypair_value(
        public_key,
        crypto_sign_PUBLICKEYBYTES,
        secret_key,
        crypto_sign_SECRETKEYBYTES
    );
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_sign_seed_keypair,
    "sign-seed-keypair",
    "(seed)",
    "Return a deterministic crypto_sign keypair from seed."
){
    LT_Value cursor = arguments;
    LT_ByteVector* seed;
    uint8_t public_key[crypto_sign_PUBLICKEYBYTES];
    uint8_t secret_key[crypto_sign_SECRETKEYBYTES];

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, seed, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    expect_bytevector_length(seed, crypto_sign_SEEDBYTES, "Sign seed length out of range");
    crypto_sign_seed_keypair(public_key, secret_key, LT_ByteVector_bytes(seed));
    return sodium_keypair_value(
        public_key,
        crypto_sign_PUBLICKEYBYTES,
        secret_key,
        crypto_sign_SECRETKEYBYTES
    );
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_sign_detached,
    "sign-detached",
    "(message secret-key)",
    "Return a detached crypto_sign signature."
){
    LT_Value cursor = arguments;
    LT_ByteVector* message;
    LT_ByteVector* secret_key;
    uint8_t* signature;
    unsigned long long signature_length;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, message, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, secret_key, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    expect_bytevector_length(secret_key, crypto_sign_SECRETKEYBYTES, "Sign secret key length out of range");

    signature = sodium_output_bytes(crypto_sign_BYTES);
    crypto_sign_detached(
        signature,
        &signature_length,
        LT_ByteVector_bytes(message),
        LT_ByteVector_length(message),
        LT_ByteVector_bytes(secret_key)
    );
    return (LT_Value)(uintptr_t)LT_ByteVector_new(
        signature,
        (size_t)signature_length
    );
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_sign_verify_detached,
    "sign-verify-detached",
    "(signature message public-key)",
    "Return true when a detached crypto_sign signature is valid."
){
    LT_Value cursor = arguments;
    LT_ByteVector* signature;
    LT_ByteVector* message;
    LT_ByteVector* public_key;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, signature, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, message, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, public_key, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    expect_bytevector_length(signature, crypto_sign_BYTES, "Signature length out of range");
    expect_bytevector_length(public_key, crypto_sign_PUBLICKEYBYTES, "Sign public key length out of range");

    return crypto_sign_verify_detached(
        LT_ByteVector_bytes(signature),
        LT_ByteVector_bytes(message),
        LT_ByteVector_length(message),
        LT_ByteVector_bytes(public_key)
    ) == 0 ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_sign,
    "sign",
    "(message secret-key)",
    "Return crypto_sign signed-message bytes."
){
    LT_Value cursor = arguments;
    LT_ByteVector* message;
    LT_ByteVector* secret_key;
    size_t message_length;
    uint8_t* signed_message;
    unsigned long long signed_message_length;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, message, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, secret_key, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    expect_bytevector_length(secret_key, crypto_sign_SECRETKEYBYTES, "Sign secret key length out of range");

    message_length = LT_ByteVector_length(message);
    signed_message = sodium_output_bytes(checked_added_length(
        message_length,
        crypto_sign_BYTES,
        "Signed message length out of range"
    ));
    crypto_sign(
        signed_message,
        &signed_message_length,
        LT_ByteVector_bytes(message),
        message_length,
        LT_ByteVector_bytes(secret_key)
    );
    return (LT_Value)(uintptr_t)LT_ByteVector_new(
        signed_message,
        (size_t)signed_message_length
    );
}

LT_DEFINE_PRIMITIVE(
    primitive_sodium_sign_open,
    "sign-open",
    "(signed-message public-key)",
    "Verify and open crypto_sign signed-message bytes, or return false."
){
    LT_Value cursor = arguments;
    LT_ByteVector* signed_message;
    LT_ByteVector* public_key;
    size_t signed_message_length;
    uint8_t* message;
    unsigned long long message_length;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, signed_message, LT_ByteVector*, LT_ByteVector_from_value);
    LT_GENERIC_ARG(cursor, public_key, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    expect_bytevector_length(public_key, crypto_sign_PUBLICKEYBYTES, "Sign public key length out of range");

    signed_message_length = LT_ByteVector_length(signed_message);
    if (signed_message_length < crypto_sign_BYTES){
        return LT_FALSE;
    }
    message = sodium_output_bytes(signed_message_length - crypto_sign_BYTES);
    if (crypto_sign_open(
            message,
            &message_length,
            LT_ByteVector_bytes(signed_message),
            signed_message_length,
            LT_ByteVector_bytes(public_key)
        ) != 0){
        return LT_FALSE;
    }
    return (LT_Value)(uintptr_t)LT_ByteVector_new(message, (size_t)message_length);
}

static LT_Primitive* sodium_primitives[] = {
    &primitive_sodium_randombytes,
    &primitive_sodium_memzero_bang,
    &primitive_sodium_memcmp,
    &primitive_sodium_generichash,
    &primitive_sodium_generichash_keygen,
    &primitive_sodium_shorthash,
    &primitive_sodium_shorthash_keygen,
    &primitive_sodium_auth,
    &primitive_sodium_auth_verify,
    &primitive_sodium_auth_keygen,
    &primitive_sodium_secretbox_keygen,
    &primitive_sodium_secretbox_nonce,
    &primitive_sodium_secretbox_easy,
    &primitive_sodium_secretbox_open_easy,
    &primitive_sodium_box_keypair,
    &primitive_sodium_box_seed_keypair,
    &primitive_sodium_box_nonce,
    &primitive_sodium_box_easy,
    &primitive_sodium_box_open_easy,
    &primitive_sodium_box_seal,
    &primitive_sodium_box_seal_open,
    &primitive_sodium_sign_keypair,
    &primitive_sodium_sign_seed_keypair,
    &primitive_sodium_sign_detached,
    &primitive_sodium_sign_verify_detached,
    &primitive_sodium_sign,
    &primitive_sodium_sign_open,
    &primitive_sodium_ristretto255_scalar_random,
    &primitive_sodium_ristretto255_scalar_reduce,
    NULL
};

void ListTalk_crypto_sodium_load(LT_Environment* environment){
    LT_Package* package = LT_Package_new("ListTalk:crypto-sodium");
    size_t i;

    if (sodium_init() < 0){
        LT_error("Could not initialize libsodium");
    }

    bind_sodium_constant(
        environment,
        package,
        "version",
        (LT_Value)(uintptr_t)LT_String_new_cstr((char*)sodium_version_string())
    );
    bind_sodium_constant(environment, package, "generichash-bytes", sodium_size_value(crypto_generichash_BYTES));
    bind_sodium_constant(environment, package, "generichash-keybytes", sodium_size_value(crypto_generichash_KEYBYTES));
    bind_sodium_constant(environment, package, "shorthash-bytes", sodium_size_value(crypto_shorthash_BYTES));
    bind_sodium_constant(environment, package, "shorthash-keybytes", sodium_size_value(crypto_shorthash_KEYBYTES));
    bind_sodium_constant(environment, package, "auth-bytes", sodium_size_value(crypto_auth_BYTES));
    bind_sodium_constant(environment, package, "auth-keybytes", sodium_size_value(crypto_auth_KEYBYTES));
    bind_sodium_constant(environment, package, "secretbox-keybytes", sodium_size_value(crypto_secretbox_KEYBYTES));
    bind_sodium_constant(environment, package, "secretbox-noncebytes", sodium_size_value(crypto_secretbox_NONCEBYTES));
    bind_sodium_constant(environment, package, "secretbox-macbytes", sodium_size_value(crypto_secretbox_MACBYTES));
    bind_sodium_constant(environment, package, "box-publickeybytes", sodium_size_value(crypto_box_PUBLICKEYBYTES));
    bind_sodium_constant(environment, package, "box-secretkeybytes", sodium_size_value(crypto_box_SECRETKEYBYTES));
    bind_sodium_constant(environment, package, "box-seedbytes", sodium_size_value(crypto_box_SEEDBYTES));
    bind_sodium_constant(environment, package, "box-noncebytes", sodium_size_value(crypto_box_NONCEBYTES));
    bind_sodium_constant(environment, package, "box-macbytes", sodium_size_value(crypto_box_MACBYTES));
    bind_sodium_constant(environment, package, "box-sealbytes", sodium_size_value(crypto_box_SEALBYTES));
    bind_sodium_constant(environment, package, "sign-publickeybytes", sodium_size_value(crypto_sign_PUBLICKEYBYTES));
    bind_sodium_constant(environment, package, "sign-secretkeybytes", sodium_size_value(crypto_sign_SECRETKEYBYTES));
    bind_sodium_constant(environment, package, "sign-seedbytes", sodium_size_value(crypto_sign_SEEDBYTES));
    bind_sodium_constant(environment, package, "sign-bytes", sodium_size_value(crypto_sign_BYTES));
    bind_sodium_constant(environment, package, "ristretto255-element-bytes", sodium_size_value(crypto_core_ristretto255_BYTES));
    bind_sodium_constant(environment, package, "ristretto255-hashbytes", sodium_size_value(crypto_core_ristretto255_HASHBYTES));
    bind_sodium_constant(environment, package, "ristretto255-scalarbytes", sodium_size_value(crypto_core_ristretto255_SCALARBYTES));
    bind_sodium_constant(environment, package, "ristretto255-nonreduced-scalarbytes", sodium_size_value(crypto_core_ristretto255_NONREDUCEDSCALARBYTES));

    for (i = 0; sodium_primitives[i] != NULL; i++){
        bind_sodium_primitive(environment, package, sodium_primitives[i]);
    }
    LT_Environment_bind(
        environment,
        LT_Symbol_new_in(package, "Ristretto255Element"),
        LT_STATIC_CLASS(LT_Ristretto255Element),
        LT_ENV_BINDING_FLAG_CONSTANT
    );
    LT_loader_provide(environment, "crypto-sodium");
}
