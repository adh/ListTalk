/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Character.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/macros/decl_macros.h>

#include <ctype.h>
#include <inttypes.h>

struct LT_Character_s {
    LT_Object base;
};

static void Character_debugPrintOn(LT_Value obj, FILE* stream){
    uint32_t value = LT_Character_value(obj);

    fputs("#\\", stream);
    switch (value){
        case '\n':
            fputs("newline", stream);
            return;
        case '\r':
            fputs("return", stream);
            return;
        case '\t':
            fputs("tab", stream);
            return;
        case ' ':
            fputs("space", stream);
            return;
        default:
            if (value <= UINT32_C(0x7f) && isprint((int)value)){
                fputc((int)value, stream);
                return;
            }
            fprintf(stream, "u+%04" PRIx32, value);
            return;
    }
}

LT_DEFINE_CLASS(LT_Character) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Character",
    .instance_size = sizeof(LT_Character),
    .class_flags = LT_CLASS_FLAG_SPECIAL | LT_CLASS_FLAG_IMMUTABLE
        | LT_CLASS_FLAG_SCALAR,
    .debugPrintOn = Character_debugPrintOn,
};
