/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/SpecialForm.h>
#include <ListTalk/vm/Class.h>
#include <ListTalk/utils.h>

struct LT_SpecialForm_s {
    LT_SpecialForm_Func function;
    char* name;
};

static void SpecialForm_debugPrintOn(LT_Value obj, FILE* stream){
    LT_SpecialForm* special_form = LT_SpecialForm_from_object(obj);
    if (special_form->name == NULL){
        fputs("#<SpecialForm>", stream);
        return;
    }
    fputs("#<SpecialForm ", stream);
    fputs(special_form->name, stream);
    fputc('>', stream);
}

LT_DEFINE_CLASS(LT_SpecialForm) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "SpecialForm",
    .instance_size = sizeof(LT_SpecialForm),
    .class_flags = LT_CLASS_FLAG_SPECIAL,
    .debugPrintOn = SpecialForm_debugPrintOn,
};

LT_Value LT_SpecialForm_new(char* name, LT_SpecialForm_Func function){
    LT_SpecialForm* special_form = GC_NEW(LT_SpecialForm);
    special_form->function = function;
    if (name == NULL){
        special_form->name = NULL;
    } else {
        special_form->name = LT_strdup(name);
    }
    return ((LT_Value)(uintptr_t)special_form) | LT_VALUE_POINTER_TAG_SPECIAL_FORM;
}

char* LT_SpecialForm_name(LT_SpecialForm* special_form){
    return special_form->name;
}

LT_SpecialForm_Func LT_SpecialForm_function(LT_SpecialForm* special_form){
    return special_form->function;
}

LT_Value LT_SpecialForm_apply(LT_Value special_form,
                              LT_Value arguments,
                              LT_Environment* environment){
    LT_SpecialForm_Func function = LT_SpecialForm_function(
        LT_SpecialForm_from_object(special_form)
    );
    return function(arguments, environment);
}
