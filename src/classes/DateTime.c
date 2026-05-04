/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/DateTime.h>
#include <ListTalk/classes/Float.h>
#include <ListTalk/classes/Instant.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/RealNumber.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Character.h>
#include <ListTalk/classes/UTCDateTime.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/macros/method_macros.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/utils.h>

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct LT_DateTime_s {
    LT_Object base;
};

struct LT_UTCDateTime_s {
    LT_Object base;
    LT_Value year;
    LT_Value month;
    LT_Value day;
    LT_Value hour;
    LT_Value minute;
    LT_Value second;
    LT_Value microsecond;
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

static LT_Value whole_seconds_from_microseconds(LT_Value microseconds){
    return LT_Number_floor(microseconds_to_seconds(microseconds));
}

static int real_to_int_range(LT_Value value,
                             int min_value,
                             int max_value,
                             const char* message){
    double number;

    require_real_number(value);
    number = LT_Number_to_double(value);
    if (!isfinite(number)
        || trunc(number) != number
        || number < (double)min_value
        || number > (double)max_value){
        LT_error(message);
    }
    return (int)number;
}

static time_t real_to_time_t(LT_Value value){
    double number;

    require_real_number(value);
    number = LT_Number_to_double(value);
    if (!isfinite(number) || trunc(number) != number){
        LT_error("Expected whole seconds representable by libc");
    }
    if (number < (double)INT64_MIN || number > (double)INT64_MAX){
        LT_error("Seconds value out of range");
    }
    return (time_t)(int64_t)number;
}

static int leap_year_p(int year){
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

static int days_in_month(int year, int month){
    static const int common_year_days[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    if (month == 2 && leap_year_p(year)){
        return 29;
    }
    return common_year_days[month - 1];
}

static int64_t days_from_civil(int year, unsigned int month, unsigned int day){
    int64_t y = (int64_t)year;
    int64_t era;
    unsigned int yoe;
    unsigned int doy;
    unsigned int doe;

    y -= month <= 2;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = (unsigned int)(y - era * 400);
    doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

static LT_Value utc_datetime_whole_seconds(LT_Value value){
    int year = real_to_int_range(
        LT_UTCDateTime_year(value),
        -1000000,
        1000000,
        "Year out of supported range"
    );
    int month = real_to_int_range(
        LT_UTCDateTime_month(value),
        1,
        12,
        "Month out of range"
    );
    int day = real_to_int_range(
        LT_UTCDateTime_day(value),
        1,
        days_in_month(year, month),
        "Day out of range"
    );
    int hour = real_to_int_range(
        LT_UTCDateTime_hour(value),
        0,
        23,
        "Hour out of range"
    );
    int minute = real_to_int_range(
        LT_UTCDateTime_minute(value),
        0,
        59,
        "Minute out of range"
    );
    int second = real_to_int_range(
        LT_UTCDateTime_second(value),
        0,
        60,
        "Second out of range"
    );
    int64_t days = days_from_civil(year, (unsigned int)month, (unsigned int)day);
    int64_t seconds = days * INT64_C(86400)
        + (int64_t)hour * INT64_C(3600)
        + (int64_t)minute * INT64_C(60)
        + (int64_t)second;

    if (seconds < LT_VALUE_FIXNUM_MIN || seconds > LT_VALUE_FIXNUM_MAX){
        LT_error("Seconds value out of range");
    }
    return LT_SmallInteger_new(seconds);
}

static LT_Value utc_datetime_as_instant(LT_Value value){
    LT_Value seconds_value;
    LT_Value whole_microseconds;
    LT_Value total_microseconds;

    seconds_value = utc_datetime_whole_seconds(value);
    whole_microseconds = seconds_to_microseconds(seconds_value);
    total_microseconds = LT_Number_add2(
        whole_microseconds,
        LT_UTCDateTime_microsecond(value)
    );
    return LT_Instant_new(total_microseconds);
}

static LT_Value utc_datetime_from_instant(LT_Value instant){
    LT_Value microseconds;
    LT_Value seconds_value;
    LT_Value whole_microseconds;
    LT_Value remainder;
    time_t whole_seconds;
    struct tm tm;

    if (!LT_Instant_p(instant)){
        LT_type_error(instant, &LT_Instant_class);
    }

    microseconds = LT_Instant_microseconds(instant);
    seconds_value = whole_seconds_from_microseconds(microseconds);
    whole_microseconds = seconds_to_microseconds(seconds_value);
    remainder = LT_Number_subtract2(microseconds, whole_microseconds);
    whole_seconds = real_to_time_t(seconds_value);

    if (gmtime_r(&whole_seconds, &tm) == NULL){
        LT_error("Unable to convert Instant with libc");
    }

    return LT_UTCDateTime_new(
        LT_SmallInteger_new((int64_t)tm.tm_year + 1900),
        LT_SmallInteger_new((int64_t)tm.tm_mon + 1),
        LT_SmallInteger_new((int64_t)tm.tm_mday),
        LT_SmallInteger_new((int64_t)tm.tm_hour),
        LT_SmallInteger_new((int64_t)tm.tm_min),
        LT_SmallInteger_new((int64_t)tm.tm_sec),
        remainder
    );
}

static LT_Slot_Descriptor UTCDateTime_slots[] = {
    {"year", offsetof(LT_UTCDateTime, year), &LT_SlotType_ReadonlyObject},
    {"month", offsetof(LT_UTCDateTime, month), &LT_SlotType_ReadonlyObject},
    {"day", offsetof(LT_UTCDateTime, day), &LT_SlotType_ReadonlyObject},
    {"hour", offsetof(LT_UTCDateTime, hour), &LT_SlotType_ReadonlyObject},
    {"minute", offsetof(LT_UTCDateTime, minute), &LT_SlotType_ReadonlyObject},
    {"second", offsetof(LT_UTCDateTime, second), &LT_SlotType_ReadonlyObject},
    {"microsecond", offsetof(LT_UTCDateTime, microsecond), &LT_SlotType_ReadonlyObject},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static size_t UTCDateTime_hash(LT_Value value){
    size_t hash = LT_Value_hash(LT_UTCDateTime_year(value));

    hash ^= LT_Value_hash(LT_UTCDateTime_month(value)) << 1;
    hash ^= LT_Value_hash(LT_UTCDateTime_day(value)) << 2;
    hash ^= LT_Value_hash(LT_UTCDateTime_hour(value)) << 3;
    hash ^= LT_Value_hash(LT_UTCDateTime_minute(value)) << 4;
    hash ^= LT_Value_hash(LT_UTCDateTime_second(value)) << 5;
    hash ^= LT_Value_hash(LT_UTCDateTime_microsecond(value)) << 6;
    return hash;
}

static int number_fields_equal(LT_Value left, LT_Value right){
    return LT_Number_equal_p(left, right);
}

static int UTCDateTime_equal_p(LT_Value self, LT_Value other){
    return LT_UTCDateTime_p(other)
        && number_fields_equal(LT_UTCDateTime_year(self), LT_UTCDateTime_year(other))
        && number_fields_equal(LT_UTCDateTime_month(self), LT_UTCDateTime_month(other))
        && number_fields_equal(LT_UTCDateTime_day(self), LT_UTCDateTime_day(other))
        && number_fields_equal(LT_UTCDateTime_hour(self), LT_UTCDateTime_hour(other))
        && number_fields_equal(LT_UTCDateTime_minute(self), LT_UTCDateTime_minute(other))
        && number_fields_equal(LT_UTCDateTime_second(self), LT_UTCDateTime_second(other))
        && number_fields_equal(LT_UTCDateTime_microsecond(self), LT_UTCDateTime_microsecond(other));
}

static void append_utf8_codepoint(LT_StringBuilder* builder, uint32_t codepoint){
    if (codepoint <= UINT32_C(0x7f)){
        LT_StringBuilder_append_char(builder, (char)codepoint);
    } else if (codepoint <= UINT32_C(0x7ff)){
        LT_StringBuilder_append_char(builder, (char)(0xc0 | (codepoint >> 6)));
        LT_StringBuilder_append_char(builder, (char)(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= UINT32_C(0xffff)){
        LT_StringBuilder_append_char(builder, (char)(0xe0 | (codepoint >> 12)));
        LT_StringBuilder_append_char(builder, (char)(0x80 | ((codepoint >> 6) & 0x3f)));
        LT_StringBuilder_append_char(builder, (char)(0x80 | (codepoint & 0x3f)));
    } else {
        LT_StringBuilder_append_char(builder, (char)(0xf0 | (codepoint >> 18)));
        LT_StringBuilder_append_char(builder, (char)(0x80 | ((codepoint >> 12) & 0x3f)));
        LT_StringBuilder_append_char(builder, (char)(0x80 | ((codepoint >> 6) & 0x3f)));
        LT_StringBuilder_append_char(builder, (char)(0x80 | (codepoint & 0x3f)));
    }
}

static void append_separator(LT_StringBuilder* builder, LT_Value separator){
    if (LT_Character_p(separator)){
        append_utf8_codepoint(builder, LT_Character_value(separator));
        return;
    }
    if (LT_String_p(separator)){
        LT_StringBuilder_append_str(
            builder,
            (char*)LT_String_value_cstr(LT_String_from_value(separator))
        );
        return;
    }
    LT_type_error(separator, &LT_String_class);
}

static int microsecond_is_zero(LT_Value microsecond){
    return LT_Number_zero_p(microsecond);
}

static void append_fractional_second(LT_StringBuilder* builder, LT_Value microsecond){
    double value;
    char* text;
    size_t length;

    if (microsecond_is_zero(microsecond)){
        return;
    }

    value = LT_Number_to_double(microsecond);
    if (!isfinite(value) || value < 0.0 || value >= 1000000.0){
        LT_error("Microsecond out of range");
    }

    text = LT_sprintf("%.12f", value / 1000000.0);
    length = strlen(text);
    while (length > 0 && text[length - 1] == '0'){
        text[--length] = '\0';
    }
    if (length > 0 && text[length - 1] == '.'){
        text[--length] = '\0';
    }
    if (text[0] == '0' && text[1] == '.'){
        LT_StringBuilder_append_str(builder, text + 1);
    }
}

static LT_Value utc_datetime_as_iso8601_with_separator(LT_Value value,
                                                       LT_Value separator){
    LT_StringBuilder* builder = LT_StringBuilder_new();

    LT_StringBuilder_append_str(
        builder,
        LT_sprintf(
            "%04d-%02d-%02d",
            real_to_int_range(LT_UTCDateTime_year(value), 0, 9999, "Year out of range"),
            real_to_int_range(LT_UTCDateTime_month(value), 1, 12, "Month out of range"),
            real_to_int_range(LT_UTCDateTime_day(value), 1, 31, "Day out of range")
        )
    );
    append_separator(builder, separator);
    LT_StringBuilder_append_str(
        builder,
        LT_sprintf(
            "%02d:%02d:%02d",
            real_to_int_range(LT_UTCDateTime_hour(value), 0, 23, "Hour out of range"),
            real_to_int_range(LT_UTCDateTime_minute(value), 0, 59, "Minute out of range"),
            real_to_int_range(LT_UTCDateTime_second(value), 0, 60, "Second out of range")
        )
    );
    append_fractional_second(builder, LT_UTCDateTime_microsecond(value));
    LT_StringBuilder_append_char(builder, 'Z');

    return (LT_Value)(uintptr_t)LT_String_new_cstr(LT_StringBuilder_value(builder));
}

static int parse_2digits(const char* text, size_t offset){
    if (text[offset] < '0' || text[offset] > '9'
        || text[offset + 1] < '0' || text[offset + 1] > '9'){
        LT_error("Invalid ISO8601 date-time");
    }
    return (text[offset] - '0') * 10 + (text[offset + 1] - '0');
}

static int parse_4digits(const char* text, size_t offset){
    return parse_2digits(text, offset) * 100 + parse_2digits(text, offset + 2);
}

static LT_Value parse_fractional_microsecond(const char* digits, size_t length){
    int64_t value = 0;
    size_t i;

    if (length == 0){
        LT_error("Invalid ISO8601 date-time");
    }

    if (length <= 6){
        for (i = 0; i < length; i++){
            if (digits[i] < '0' || digits[i] > '9'){
                LT_error("Invalid ISO8601 date-time");
            }
            value = value * 10 + (int64_t)(digits[i] - '0');
        }
        while (length < 6){
            value *= 10;
            length++;
        }
        return LT_SmallInteger_new(value);
    }

    {
        char* buffer = GC_MALLOC_ATOMIC(length + 3);
        double fraction;

        buffer[0] = '0';
        buffer[1] = '.';
        memcpy(buffer + 2, digits, length);
        buffer[length + 2] = '\0';
        for (i = 0; i < length; i++){
            if (digits[i] < '0' || digits[i] > '9'){
                LT_error("Invalid ISO8601 date-time");
            }
        }
        fraction = strtod(buffer, NULL);
        return LT_Float_new(fraction * 1000000.0);
    }
}

static LT_Value parse_iso8601_utc_datetime(LT_String* string){
    const char* text = LT_String_value_cstr(string);
    size_t length = LT_String_byte_length(string);
    size_t cursor = 19;
    LT_Value microsecond = LT_SmallInteger_new(0);

    if (length < 20
        || text[4] != '-'
        || text[7] != '-'
        || text[10] != 'T'
        || text[13] != ':'
        || text[16] != ':'){
        LT_error("Invalid ISO8601 date-time");
    }

    if (text[cursor] == '.'){
        size_t start;

        cursor++;
        start = cursor;
        while (cursor < length && text[cursor] >= '0' && text[cursor] <= '9'){
            cursor++;
        }
        microsecond = parse_fractional_microsecond(text + start, cursor - start);
    }

    if (cursor + 1 != length || text[cursor] != 'Z'){
        LT_error("Invalid ISO8601 date-time");
    }

    return LT_UTCDateTime_new(
        LT_SmallInteger_new(parse_4digits(text, 0)),
        LT_SmallInteger_new(parse_2digits(text, 5)),
        LT_SmallInteger_new(parse_2digits(text, 8)),
        LT_SmallInteger_new(parse_2digits(text, 11)),
        LT_SmallInteger_new(parse_2digits(text, 14)),
        LT_SmallInteger_new(parse_2digits(text, 17)),
        microsecond
    );
}

LT_DEFINE_PRIMITIVE(
    utcDateTime_class_method_from_fields,
    "UTCDateTime class>>fromYear:month:day:hour:minute:second:microsecond:",
    "(self year month day hour minute second microsecond)",
    "Return UTC date-time from component fields."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value year;
    LT_Value month;
    LT_Value day;
    LT_Value hour;
    LT_Value minute;
    LT_Value second;
    LT_Value microsecond;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, year);
    LT_OBJECT_ARG(cursor, month);
    LT_OBJECT_ARG(cursor, day);
    LT_OBJECT_ARG(cursor, hour);
    LT_OBJECT_ARG(cursor, minute);
    LT_OBJECT_ARG(cursor, second);
    LT_OBJECT_ARG(cursor, microsecond);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_UTCDateTime_class){
        LT_error("fromYear:month:day:hour:minute:second:microsecond: class method is only supported on UTCDateTime");
    }
    return LT_UTCDateTime_new(year, month, day, hour, minute, second, microsecond);
}

LT_DEFINE_PRIMITIVE(
    utcDateTime_class_method_from_instant,
    "UTCDateTime class>>fromInstant:",
    "(self instant)",
    "Return UTC date-time representing instant."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value instant;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, instant);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_UTCDateTime_class){
        LT_error("fromInstant: class method is only supported on UTCDateTime");
    }
    return utc_datetime_from_instant(instant);
}

LT_DEFINE_PRIMITIVE(
    utcDateTime_class_method_from_iso8601,
    "UTCDateTime class>>fromISO8601:",
    "(self string)",
    "Return UTC date-time parsed from simple ISO8601 text."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* string;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, string, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_UTCDateTime_class){
        LT_error("fromISO8601: class method is only supported on UTCDateTime");
    }
    return parse_iso8601_utc_datetime(string);
}

#define UTC_DATE_TIME_ACCESSOR(name)                                      \
    LT_DEFINE_PRIMITIVE_FLAGS(                                            \
        utcDateTime_method_##name,                                        \
        "UTCDateTime>>" #name,                                           \
        "(self)",                                                        \
        "Return UTC date-time " #name ".",                               \
        LT_PRIMITIVE_FLAG_PURE                                           \
    ){                                                                   \
        LT_Value cursor = arguments;                                     \
        LT_Value self;                                                   \
        (void)tail_call_unwind_marker;                                   \
                                                                         \
        LT_OBJECT_ARG(cursor, self);                                     \
        LT_ARG_END(cursor);                                              \
        return LT_UTCDateTime_##name(self);                              \
    }

UTC_DATE_TIME_ACCESSOR(year)
UTC_DATE_TIME_ACCESSOR(month)
UTC_DATE_TIME_ACCESSOR(day)
UTC_DATE_TIME_ACCESSOR(hour)
UTC_DATE_TIME_ACCESSOR(minute)
UTC_DATE_TIME_ACCESSOR(second)
UTC_DATE_TIME_ACCESSOR(microsecond)

LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    dateTime_method_year,
    "DateTime>>year",
    "Return date-time year."
)
LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    dateTime_method_month,
    "DateTime>>month",
    "Return date-time month."
)
LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    dateTime_method_day,
    "DateTime>>day",
    "Return date-time day."
)
LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    dateTime_method_hour,
    "DateTime>>hour",
    "Return date-time hour."
)
LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    dateTime_method_minute,
    "DateTime>>minute",
    "Return date-time minute."
)
LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    dateTime_method_second,
    "DateTime>>second",
    "Return date-time second."
)
LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    dateTime_method_microsecond,
    "DateTime>>microsecond",
    "Return date-time microsecond."
)
LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    dateTime_method_as_instant,
    "DateTime>>asInstant",
    "Return receiver converted to an Instant."
)
LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_0(
    dateTime_method_as_iso8601,
    "DateTime>>asISO8601",
    "Return receiver formatted as ISO8601 text."
)
LT_DEFINE_SUBCLASS_RESPONSIBILITY_METHOD_1(
    dateTime_method_as_iso8601_with_separator,
    "DateTime>>asISO8601WithSeparator:",
    separator,
    "Return receiver formatted as ISO8601 text with separator."
)

LT_DEFINE_PRIMITIVE_FLAGS(
    utcDateTime_method_as_instant,
    "UTCDateTime>>asInstant",
    "(self)",
    "Return receiver converted to an Instant.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return utc_datetime_as_instant(self);
}

LT_DEFINE_PRIMITIVE_FLAGS(
    utcDateTime_method_as_iso8601,
    "UTCDateTime>>asISO8601",
    "(self)",
    "Return receiver formatted as ISO8601 UTC text.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return utc_datetime_as_iso8601_with_separator(self, LT_Character_new((uint32_t)'T'));
}

LT_DEFINE_PRIMITIVE_FLAGS(
    utcDateTime_method_as_iso8601_with_separator,
    "UTCDateTime>>asISO8601WithSeparator:",
    "(self separator)",
    "Return receiver formatted as ISO8601 UTC text with separator.",
    LT_PRIMITIVE_FLAG_PURE
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value separator;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, separator);
    LT_ARG_END(cursor);
    return utc_datetime_as_iso8601_with_separator(self, separator);
}

static LT_Method_Descriptor UTCDateTime_methods[] = {
    {"year", &utcDateTime_method_year},
    {"month", &utcDateTime_method_month},
    {"day", &utcDateTime_method_day},
    {"hour", &utcDateTime_method_hour},
    {"minute", &utcDateTime_method_minute},
    {"second", &utcDateTime_method_second},
    {"microsecond", &utcDateTime_method_microsecond},
    {"asInstant", &utcDateTime_method_as_instant},
    {"asISO8601", &utcDateTime_method_as_iso8601},
    {"asISO8601WithSeparator:", &utcDateTime_method_as_iso8601_with_separator},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor UTCDateTime_class_methods[] = {
    {"fromYear:month:day:hour:minute:second:microsecond:", &utcDateTime_class_method_from_fields},
    {"fromInstant:", &utcDateTime_class_method_from_instant},
    {"fromISO8601:", &utcDateTime_class_method_from_iso8601},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor DateTime_methods[] = {
    {"year", &dateTime_method_year},
    {"month", &dateTime_method_month},
    {"day", &dateTime_method_day},
    {"hour", &dateTime_method_hour},
    {"minute", &dateTime_method_minute},
    {"second", &dateTime_method_second},
    {"microsecond", &dateTime_method_microsecond},
    {"asInstant", &dateTime_method_as_instant},
    {"asISO8601", &dateTime_method_as_iso8601},
    {"asISO8601WithSeparator:", &dateTime_method_as_iso8601_with_separator},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_DateTime) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "DateTime",
    .instance_size = sizeof(LT_DateTime),
    .methods = DateTime_methods,
    .class_flags = LT_CLASS_FLAG_ABSTRACT | LT_CLASS_FLAG_IMMUTABLE | LT_CLASS_FLAG_SCALAR,
};

LT_DEFINE_CLASS(LT_UTCDateTime) {
    .superclass = &LT_DateTime_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "UTCDateTime",
    .instance_size = sizeof(LT_UTCDateTime),
    .hash = UTCDateTime_hash,
    .equal_p = UTCDateTime_equal_p,
    .class_flags = LT_CLASS_FLAG_IMMUTABLE | LT_CLASS_FLAG_SCALAR,
    .slots = UTCDateTime_slots,
    .methods = UTCDateTime_methods,
    .class_methods = UTCDateTime_class_methods,
};

LT_Value LT_UTCDateTime_new(LT_Value year,
                            LT_Value month,
                            LT_Value day,
                            LT_Value hour,
                            LT_Value minute,
                            LT_Value second,
                            LT_Value microsecond){
    LT_UTCDateTime* date_time;

    require_real_number(year);
    require_real_number(month);
    require_real_number(day);
    require_real_number(hour);
    require_real_number(minute);
    require_real_number(second);
    require_real_number(microsecond);
    date_time = LT_Class_ALLOC(LT_UTCDateTime);
    date_time->year = year;
    date_time->month = month;
    date_time->day = day;
    date_time->hour = hour;
    date_time->minute = minute;
    date_time->second = second;
    date_time->microsecond = microsecond;
    return (LT_Value)(uintptr_t)date_time;
}

LT_Value LT_UTCDateTime_year(LT_Value value){
    return LT_UTCDateTime_from_value(value)->year;
}

LT_Value LT_UTCDateTime_month(LT_Value value){
    return LT_UTCDateTime_from_value(value)->month;
}

LT_Value LT_UTCDateTime_day(LT_Value value){
    return LT_UTCDateTime_from_value(value)->day;
}

LT_Value LT_UTCDateTime_hour(LT_Value value){
    return LT_UTCDateTime_from_value(value)->hour;
}

LT_Value LT_UTCDateTime_minute(LT_Value value){
    return LT_UTCDateTime_from_value(value)->minute;
}

LT_Value LT_UTCDateTime_second(LT_Value value){
    return LT_UTCDateTime_from_value(value)->second;
}

LT_Value LT_UTCDateTime_microsecond(LT_Value value){
    return LT_UTCDateTime_from_value(value)->microsecond;
}
