/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__SpecialForm__
#define H__ListTalk__SpecialForm__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/vm/Environment.h>
#include <ListTalk/vm/tail_call.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_SpecialForm);

typedef LT_Value(*LT_SpecialForm_Func)(
    LT_Value arguments,
    LT_Environment* environment,
    LT_TailCallUnwindMarker* tail_call_unwind_marker
);

struct LT_SpecialForm_s {
    LT_SpecialForm_Func function;
    char* name;
    char* arguments;
    char* description;
};

LT_Value LT_SpecialForm_new(char* name,
                            char* arguments,
                            char* description,
                            LT_SpecialForm_Func function);
LT_Value LT_SpecialForm_from_static(LT_SpecialForm* special_form);
char* LT_SpecialForm_name(LT_SpecialForm* special_form);
char* LT_SpecialForm_arguments(LT_SpecialForm* special_form);
char* LT_SpecialForm_description(LT_SpecialForm* special_form);
LT_SpecialForm_Func LT_SpecialForm_function(LT_SpecialForm* special_form);
LT_Value LT_SpecialForm_apply(
    LT_Value special_form,
    LT_Value arguments,
    LT_Environment* environment,
    LT_TailCallUnwindMarker* tail_call_unwind_marker
);

LT__END_DECLS

#endif
