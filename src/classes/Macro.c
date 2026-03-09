/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Macro.h>
#include <ListTalk/vm/Class.h>

struct LT_Macro_s {
    LT_Value callable;
};

static void Macro_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Macro* macro = LT_Macro_from_value(obj);

    fputs("#<Macro ", stream);
    LT_Value_debugPrintOn(macro->callable, stream);
    fputc('>', stream);
}

LT_DEFINE_CLASS(LT_Macro) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Macro",
    .instance_size = sizeof(LT_Macro),
    .class_flags = LT_CLASS_FLAG_SPECIAL,
    .debugPrintOn = Macro_debugPrintOn,
};

LT_Value LT_Macro_new(LT_Value callable){
    LT_Macro* macro = GC_NEW(LT_Macro);
    macro->callable = callable;
    return ((LT_Value)(uintptr_t)macro) | LT_VALUE_POINTER_TAG_MACRO;
}

LT_Value LT_Macro_callable(LT_Macro* macro){
    return macro->callable;
}
