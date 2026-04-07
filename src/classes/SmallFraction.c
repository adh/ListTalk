/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/SmallFraction.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/macros/decl_macros.h>

#include <inttypes.h>

struct LT_SmallFraction_s {
    LT_Object base;
};

static void SmallFraction_debugPrintOn(LT_Value obj, FILE* stream){
    if (LT_SmallFraction_p(obj)){
        fprintf(
            stream,
            "%" PRId64 "/%" PRIu32,
            LT_SmallFraction_numerator(obj),
            LT_SmallFraction_denominator(obj)
        );
        return;
    }

    fprintf(stream, "#<small-fraction 0x%" PRIxPTR ">", (uintptr_t)obj);
}

LT_DEFINE_CLASS(LT_SmallFraction) {
    .superclass = &LT_Number_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "SmallFraction",
    .instance_size = sizeof(LT_SmallFraction),
    .class_flags = LT_CLASS_FLAG_SPECIAL | LT_CLASS_FLAG_IMMUTABLE
        | LT_CLASS_FLAG_SCALAR,
    .debugPrintOn = SmallFraction_debugPrintOn,
};
