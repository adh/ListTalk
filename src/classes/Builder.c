/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Builder.h>

#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/Character.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Object.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/utils.h>
#include <ListTalk/utils/utf8.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>

#include <stdint.h>
#include <string.h>

#define BYTEVECTOR_BUILDER_INITIAL_CAPACITY 64

struct LT_StringBuilderObject_s {
    LT_Object base;
    LT_ListBuilder* elements;
};

struct LT_ByteVectorBuilder_s {
    LT_Object base;
    size_t length;
    size_t capacity;
    uint8_t* bytes;
};

static LT_StringBuilderObject* StringBuilder_new(void){
    LT_StringBuilderObject* builder = LT_Class_ALLOC(LT_StringBuilderObject);

    builder->elements = LT_ListBuilder_new();
    return builder;
}

static LT_ByteVectorBuilder* ByteVectorBuilder_new(void){
    LT_ByteVectorBuilder* builder = LT_Class_ALLOC(LT_ByteVectorBuilder);

    builder->length = 0;
    builder->capacity = BYTEVECTOR_BUILDER_INITIAL_CAPACITY;
    builder->bytes = GC_MALLOC_ATOMIC(builder->capacity);
    return builder;
}

static void ByteVectorBuilder_reserve(LT_ByteVectorBuilder* builder,
                                      size_t additional_length){
    size_t required_capacity;
    size_t new_capacity;
    uint8_t* new_bytes;

    if (additional_length > SIZE_MAX - builder->length){
        LT_error("ByteVectorBuilder capacity overflow");
    }
    required_capacity = builder->length + additional_length;
    if (required_capacity <= builder->capacity){
        return;
    }

    new_capacity = builder->capacity;
    while (new_capacity < required_capacity){
        if (new_capacity > (SIZE_MAX - 2) / 2){
            new_capacity = required_capacity;
            break;
        }
        new_capacity = new_capacity * 2 + 2;
    }
    new_bytes = GC_MALLOC_ATOMIC(new_capacity);
    memcpy(new_bytes, builder->bytes, builder->length);
    builder->capacity = new_capacity;
    builder->bytes = new_bytes;
}

static void ByteVectorBuilder_append_bytes(LT_ByteVectorBuilder* builder,
                                           const uint8_t* bytes,
                                           size_t length){
    ByteVectorBuilder_reserve(builder, length);
    memcpy(builder->bytes + builder->length, bytes, length);
    builder->length += length;
}

LT_DEFINE_PRIMITIVE(
    string_builder_class_method_new,
    "StringBuilder class>>new",
    "(self)",
    "Return a new empty string builder."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_StringBuilderObject_class){
        LT_error("new class method is only supported on StringBuilder");
    }
    return (LT_Value)(uintptr_t)StringBuilder_new();
}

LT_DEFINE_PRIMITIVE(
    string_builder_method_append,
    "StringBuilder>>append:",
    "(self value)",
    "Append a string or character and return the receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value value;
    LT_StringBuilderObject* builder;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);
    if (!LT_String_p(value) && !LT_Character_p(value)){
        LT_error("StringBuilder append: expects a String or Character");
    }

    builder = LT_StringBuilderObject_from_value(self);
    LT_ListBuilder_append(builder->elements, value);
    return self;
}

LT_DEFINE_PRIMITIVE(
    string_builder_method_value,
    "StringBuilder>>value",
    "(self)",
    "Return the accumulated string."
){
    LT_Value cursor = arguments;
    LT_StringBuilderObject* builder;
    LT_Value elements;
    LT_StringBuilder* result;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(
        cursor,
        builder,
        LT_StringBuilderObject*,
        LT_StringBuilderObject_from_value
    );
    LT_ARG_END(cursor);

    result = LT_StringBuilder_new();
    elements = LT_ListBuilder_value(builder->elements);
    while (elements != LT_NIL){
        LT_Value element = LT_car(elements);

        if (LT_String_p(element)){
            LT_String* string = LT_String_from_value(element);

            LT_StringBuilder_append_bytes(
                result,
                LT_String_value_cstr(string),
                LT_String_byte_length(string)
            );
        } else {
            char encoded[4];
            size_t length = LT_utf8_encode(LT_Character_value(element), encoded);

            LT_StringBuilder_append_bytes(result, encoded, length);
        }
        elements = LT_cdr(elements);
    }
    return (LT_Value)(uintptr_t)LT_String_new(
        LT_StringBuilder_value(result),
        LT_StringBuilder_length(result)
    );
}

LT_DEFINE_PRIMITIVE(
    bytevector_builder_class_method_new,
    "ByteVectorBuilder class>>new",
    "(self)",
    "Return a new empty bytevector builder."
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_ByteVectorBuilder_class){
        LT_error("new class method is only supported on ByteVectorBuilder");
    }
    return (LT_Value)(uintptr_t)ByteVectorBuilder_new();
}

LT_DEFINE_PRIMITIVE(
    bytevector_builder_method_append,
    "ByteVectorBuilder>>append:",
    "(self value)",
    "Append a bytevector, character, or byte and return the receiver."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value value;
    LT_ByteVectorBuilder* builder;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, value);
    LT_ARG_END(cursor);

    builder = LT_ByteVectorBuilder_from_value(self);
    if (LT_ByteVector_p(value)){
        LT_ByteVector* bytevector = LT_ByteVector_from_value(value);

        ByteVectorBuilder_append_bytes(
            builder,
            LT_ByteVector_bytes(bytevector),
            LT_ByteVector_length(bytevector)
        );
    } else if (LT_Character_p(value)){
        char encoded[4];
        size_t length = LT_utf8_encode(LT_Character_value(value), encoded);

        ByteVectorBuilder_append_bytes(builder, (uint8_t*)encoded, length);
    } else if (LT_Value_is_fixnum(value)){
        uint8_t byte = LT_Number_uint8_from_integer(
            value,
            "ByteVectorBuilder byte value out of range"
        );

        ByteVectorBuilder_append_bytes(builder, &byte, 1);
    } else {
        LT_error(
            "ByteVectorBuilder append: expects a ByteVector, Character, or byte"
        );
    }
    return self;
}

LT_DEFINE_PRIMITIVE(
    bytevector_builder_method_value,
    "ByteVectorBuilder>>value",
    "(self)",
    "Return a fresh bytevector containing the accumulated bytes."
){
    LT_Value cursor = arguments;
    LT_ByteVectorBuilder* builder;
    (void)tail_call_unwind_marker;

    LT_GENERIC_ARG(
        cursor,
        builder,
        LT_ByteVectorBuilder*,
        LT_ByteVectorBuilder_from_value
    );
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_ByteVector_new(
        builder->bytes,
        builder->length
    );
}

static LT_Method_Descriptor StringBuilder_methods[] = {
    {"append:", &string_builder_method_append},
    {"value", &string_builder_method_value},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor StringBuilder_class_methods[] = {
    {"new", &string_builder_class_method_new},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor ByteVectorBuilder_methods[] = {
    {"append:", &bytevector_builder_method_append},
    {"value", &bytevector_builder_method_value},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor ByteVectorBuilder_class_methods[] = {
    {"new", &bytevector_builder_class_method_new},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_StringBuilderObject) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "StringBuilder",
    .documentation = "Builder that accumulates strings and characters.",
    .instance_size = sizeof(LT_StringBuilderObject),
    .methods = StringBuilder_methods,
    .class_methods = StringBuilder_class_methods,
};

LT_DEFINE_CLASS(LT_ByteVectorBuilder) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "ByteVectorBuilder",
    .documentation = "Builder backed by a growable byte buffer.",
    .instance_size = sizeof(LT_ByteVectorBuilder),
    .methods = ByteVectorBuilder_methods,
    .class_methods = ByteVectorBuilder_class_methods,
};
