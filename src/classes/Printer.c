/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Printer.h>
#include <ListTalk/vm/Class.h>
#include <stdio.h>

struct LT_Printer_s {
    LT_Object base;
};

LT_DEFINE_CLASS(LT_Printer) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Printer",
    .instance_size = sizeof(LT_Printer),
};

void LT_printer_print_object(LT_Value object){
    LT_Value_debugPrintOn(object, stdout);
}
