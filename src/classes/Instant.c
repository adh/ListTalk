/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Duration.h>
#include <ListTalk/classes/Instant.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/RealNumber.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>

#include <stddef.h>
#include <stdint.h>
#include <time.h>

struct LT_Instant_s {
    LT_Object base;
    LT_Value microseconds;
};

struct LT_Duration_s {
    LT_Object base;
    LT_Value microseconds;
};

static LT_Value microseconds_per_second(void){
    return LT_SmallInteger_new(1000000);
}

static void require_real_number(LT_Value value){
    if (!LT_Value_is_instance_of(value, LT_STATIC_CLASS(LT_RealNumber))){
        LT_type_error(value, &LT_RealNumber_class);
    }
}

static LT_Value seconds_to_microseconds(LT_Value seconds){
    require_real_number(seconds);
    return LT_Number_multiply2(seconds, microseconds_per_second());
}

static LT_Value microseconds_to_seconds(LT_Value microseconds){
    return LT_Number_divide2(microseconds, microseconds_per_second());
}

static LT_Slot_Descriptor Instant_slots[] = {
    {"microseconds", offsetof(LT_Instant, microseconds), &LT_SlotType_ReadonlyObject},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static LT_Slot_Descriptor Duration_slots[] = {
    {"microseconds", offsetof(LT_Duration, microseconds), &LT_SlotType_ReadonlyObject},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static size_t Instant_hash(LT_Value value){
    return LT_Value_hash(LT_Instant_microseconds(value));
}

static int Instant_equal_p(LT_Value self, LT_Value other){
    return LT_Instant_p(other)
        && LT_Number_equal_p(
            LT_Instant_microseconds(self),
            LT_Instant_microseconds(other)
        );
}

static size_t Duration_hash(LT_Value value){
    return LT_Value_hash(LT_Duration_microseconds(value));
}

static int Duration_equal_p(LT_Value self, LT_Value other){
    return LT_Duration_p(other)
        && LT_Number_equal_p(
            LT_Duration_microseconds(self),
            LT_Duration_microseconds(other)
        );
}

LT_DEFINE_PRIMITIVE(
    instant_class_method_from_microseconds,
    "Instant class>>fromMicroseconds:",
    "(self microseconds)",
    "Return an instant from UTC microseconds since the Unix epoch."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value microseconds;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, microseconds);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_Instant_class){
        LT_error("fromMicroseconds: class method is only supported on Instant");
    }
    return LT_Instant_new(microseconds);
}

LT_DEFINE_PRIMITIVE(
    instant_class_method_from_seconds,
    "Instant class>>fromSeconds:",
    "(self seconds)",
    "Return an instant from UTC seconds since the Unix epoch."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value seconds;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, seconds);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_Instant_class){
        LT_error("fromSeconds: class method is only supported on Instant");
    }
    return LT_Instant_new(seconds_to_microseconds(seconds));
}

LT_DEFINE_PRIMITIVE(
    instant_class_method_now,
    "Instant class>>now",
    "(self)",
    "Return the current instant."
){
    LT_Value cursor = arguments;
    LT_Value self;
    struct timespec now;
    int64_t microseconds;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_Instant_class){
        LT_error("now class method is only supported on Instant");
    }
    if (clock_gettime(CLOCK_REALTIME, &now) != 0){
        LT_error("Unable to read current time");
    }
    microseconds = ((int64_t)now.tv_sec * INT64_C(1000000))
        + ((int64_t)now.tv_nsec / INT64_C(1000));
    return LT_Instant_new(LT_SmallInteger_new(microseconds));
}

LT_DEFINE_PRIMITIVE_FLAGS(
    instant_method_as_microseconds,
    "Instant>>asMicroseconds",
    "(self)",
    "Return receiver as UTC microseconds since the Unix epoch.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Instant_microseconds(self);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    instant_method_as_seconds,
    "Instant>>asSeconds",
    "(self)",
    "Return receiver as UTC seconds since the Unix epoch.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return microseconds_to_seconds(LT_Instant_microseconds(self));
}

LT_DEFINE_PRIMITIVE_FLAGS(
    instant_method_after_duration,
    "Instant>>afterDuration:",
    "(self duration)",
    "Return receiver advanced by duration.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value duration;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, duration);
    LT_ARG_END(cursor);
    if (!LT_Duration_p(duration)){
        LT_type_error(duration, &LT_Duration_class);
    }
    return LT_Instant_new(LT_Number_add2(
        LT_Instant_microseconds(self),
        LT_Duration_microseconds(duration)
    ));
}

LT_DEFINE_PRIMITIVE_FLAGS(
    instant_method_before_duration,
    "Instant>>beforeDuration:",
    "(self duration)",
    "Return receiver moved backward by duration.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value duration;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, duration);
    LT_ARG_END(cursor);
    if (!LT_Duration_p(duration)){
        LT_type_error(duration, &LT_Duration_class);
    }
    return LT_Instant_new(LT_Number_subtract2(
        LT_Instant_microseconds(self),
        LT_Duration_microseconds(duration)
    ));
}

LT_DEFINE_PRIMITIVE_FLAGS(
    instant_method_duration_since,
    "Instant>>durationSince:",
    "(self other)",
    "Return duration from other instant to receiver.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, other);
    LT_ARG_END(cursor);
    if (!LT_Instant_p(other)){
        LT_type_error(other, &LT_Instant_class);
    }
    return LT_Duration_new(LT_Number_subtract2(
        LT_Instant_microseconds(self),
        LT_Instant_microseconds(other)
    ));
}

LT_DEFINE_PRIMITIVE_FLAGS(
    instant_method_duration_until,
    "Instant>>durationUntil:",
    "(self other)",
    "Return duration from receiver to other instant.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, other);
    LT_ARG_END(cursor);
    if (!LT_Instant_p(other)){
        LT_type_error(other, &LT_Instant_class);
    }
    return LT_Duration_new(LT_Number_subtract2(
        LT_Instant_microseconds(other),
        LT_Instant_microseconds(self)
    ));
}

LT_DEFINE_PRIMITIVE_FLAGS(
    instant_method_add,
    "Instant>>+",
    "(self duration)",
    "Return receiver advanced by duration.",
    LT_PRIMITIVE_FLAG_PURE
){
    return instant_method_after_duration.function(
        arguments,
        invocation_context_kind,
        invocation_context_data,
        tail_call_unwind_marker
    );
}

LT_DEFINE_PRIMITIVE_FLAGS(
    instant_method_subtract,
    "Instant>>-",
    "(self duration-or-instant)",
    "Subtract a duration or return duration since another instant.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, other);
    LT_ARG_END(cursor);
    if (LT_Duration_p(other)){
        return LT_Instant_new(LT_Number_subtract2(
            LT_Instant_microseconds(self),
            LT_Duration_microseconds(other)
        ));
    }
    if (LT_Instant_p(other)){
        return LT_Duration_new(LT_Number_subtract2(
            LT_Instant_microseconds(self),
            LT_Instant_microseconds(other)
        ));
    }
    LT_type_error(other, &LT_Duration_class);
    return LT_NIL;
}

static LT_Method_Descriptor Instant_methods[] = {
    {"asMicroseconds", &instant_method_as_microseconds},
    {"asSeconds", &instant_method_as_seconds},
    {"afterDuration:", &instant_method_after_duration},
    {"beforeDuration:", &instant_method_before_duration},
    {"durationSince:", &instant_method_duration_since},
    {"durationUntil:", &instant_method_duration_until},
    {"+", &instant_method_add},
    {"-", &instant_method_subtract},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor Instant_class_methods[] = {
    {"fromMicroseconds:", &instant_class_method_from_microseconds},
    {"fromSeconds:", &instant_class_method_from_seconds},
    {"now", &instant_class_method_now},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Instant) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Instant",
    .instance_size = sizeof(LT_Instant),
    .hash = Instant_hash,
    .equal_p = Instant_equal_p,
    .class_flags = LT_CLASS_FLAG_IMMUTABLE | LT_CLASS_FLAG_SCALAR,
    .slots = Instant_slots,
    .methods = Instant_methods,
    .class_methods = Instant_class_methods,
};

LT_DEFINE_PRIMITIVE(
    duration_class_method_from_microseconds,
    "Duration class>>fromMicroseconds:",
    "(self microseconds)",
    "Return a duration from microseconds."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value microseconds;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, microseconds);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_Duration_class){
        LT_error("fromMicroseconds: class method is only supported on Duration");
    }
    return LT_Duration_new(microseconds);
}

LT_DEFINE_PRIMITIVE(
    duration_class_method_from_seconds,
    "Duration class>>fromSeconds:",
    "(self seconds)",
    "Return a duration from seconds."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value seconds;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, seconds);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_Duration_class){
        LT_error("fromSeconds: class method is only supported on Duration");
    }
    return LT_Duration_new(seconds_to_microseconds(seconds));
}

LT_DEFINE_PRIMITIVE_FLAGS(
    duration_method_as_microseconds,
    "Duration>>asMicroseconds",
    "(self)",
    "Return receiver as microseconds.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Duration_microseconds(self);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    duration_method_as_seconds,
    "Duration>>asSeconds",
    "(self)",
    "Return receiver as seconds.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return microseconds_to_seconds(LT_Duration_microseconds(self));
}

LT_DEFINE_PRIMITIVE_FLAGS(
    duration_method_add_duration,
    "Duration>>addDuration:",
    "(self other)",
    "Return sum of receiver and another duration.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, other);
    LT_ARG_END(cursor);
    if (!LT_Duration_p(other)){
        LT_type_error(other, &LT_Duration_class);
    }
    return LT_Duration_new(LT_Number_add2(
        LT_Duration_microseconds(self),
        LT_Duration_microseconds(other)
    ));
}

LT_DEFINE_PRIMITIVE_FLAGS(
    duration_method_substract_duration,
    "Duration>>substractDuration:",
    "(self other)",
    "Return difference of receiver and another duration.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, other);
    LT_ARG_END(cursor);
    if (!LT_Duration_p(other)){
        LT_type_error(other, &LT_Duration_class);
    }
    return LT_Duration_new(LT_Number_subtract2(
        LT_Duration_microseconds(self),
        LT_Duration_microseconds(other)
    ));
}

LT_DEFINE_PRIMITIVE_FLAGS(
    duration_method_add,
    "Duration>>+",
    "(self duration-or-instant)",
    "Add a duration or advance an instant by receiver.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value other;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, other);
    LT_ARG_END(cursor);
    if (LT_Duration_p(other)){
        return LT_Duration_new(LT_Number_add2(
            LT_Duration_microseconds(self),
            LT_Duration_microseconds(other)
        ));
    }
    if (LT_Instant_p(other)){
        return LT_Instant_new(LT_Number_add2(
            LT_Instant_microseconds(other),
            LT_Duration_microseconds(self)
        ));
    }
    LT_type_error(other, &LT_Duration_class);
    return LT_NIL;
}

LT_DEFINE_PRIMITIVE_FLAGS(
    duration_method_subtract,
    "Duration>>-",
    "(self other)",
    "Return difference of receiver and another duration.",
    LT_PRIMITIVE_FLAG_PURE
){
    return duration_method_substract_duration.function(
        arguments,
        invocation_context_kind,
        invocation_context_data,
        tail_call_unwind_marker
    );
}

static LT_Method_Descriptor Duration_methods[] = {
    {"asMicroseconds", &duration_method_as_microseconds},
    {"asSeconds", &duration_method_as_seconds},
    {"addDuration:", &duration_method_add_duration},
    {"substractDuration:", &duration_method_substract_duration},
    {"+", &duration_method_add},
    {"-", &duration_method_subtract},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor Duration_class_methods[] = {
    {"fromMicroseconds:", &duration_class_method_from_microseconds},
    {"fromSeconds:", &duration_class_method_from_seconds},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Duration) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Duration",
    .instance_size = sizeof(LT_Duration),
    .hash = Duration_hash,
    .equal_p = Duration_equal_p,
    .class_flags = LT_CLASS_FLAG_IMMUTABLE | LT_CLASS_FLAG_SCALAR,
    .slots = Duration_slots,
    .methods = Duration_methods,
    .class_methods = Duration_class_methods,
};

LT_Value LT_Instant_new(LT_Value microseconds){
    LT_Instant* instant;

    require_real_number(microseconds);
    instant = LT_Class_ALLOC(LT_Instant);
    instant->microseconds = microseconds;
    return (LT_Value)(uintptr_t)instant;
}

LT_Value LT_Instant_microseconds(LT_Value value){
    return LT_Instant_from_value(value)->microseconds;
}

LT_Value LT_Duration_new(LT_Value microseconds){
    LT_Duration* duration;

    require_real_number(microseconds);
    duration = LT_Class_ALLOC(LT_Duration);
    duration->microseconds = microseconds;
    return (LT_Value)(uintptr_t)duration;
}

LT_Value LT_Duration_microseconds(LT_Value value){
    return LT_Duration_from_value(value)->microseconds;
}
