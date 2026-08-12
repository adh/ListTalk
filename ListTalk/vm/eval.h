/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__vm__eval__
#define H__ListTalk__vm__eval__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/vm/value.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/classes/Environment.h>

LT__BEGIN_DECLS

typedef struct LT_TailCallUnwindMarker_s LT_TailCallUnwindMarker;

extern LT_Value LT_eval(
    LT_Value expression,
    LT_Environment* environment,
    LT_TailCallUnwindMarker* tail_call_unwind_marker
);

extern LT_Value LT_eval_sequence(
    LT_Value body,
    LT_Environment* environment,
    LT_TailCallUnwindMarker* tail_call_unwind_marker
);

extern LT_Value LT_quasiquote(
    LT_Value expression,
    LT_Environment* environment
);

extern LT_Value LT_eval_sequence_string(
    const char* source,
    LT_Environment* environment
);

extern LT_Value LT_apply(
    LT_Value callable,
    LT_Value arguments,
    LT_Value invocation_context_kind,
    LT_Value invocation_context_data,
    LT_TailCallUnwindMarker* tail_call_unwind_marker
);

extern LT_Value LT_applyv(LT_Value callable, LT_Value first, ...);

extern LT_Value LT_send(
    LT_Value receiver,
    LT_Value selector,
    LT_Value arguments,
    LT_TailCallUnwindMarker* tail_call_unwind_marker
);

typedef struct LT__SendSite {
    LT_Value selector;
} LT__SendSite;

#define LT__SEND_SITE_INITIALIZER {LT_INVALID}

static inline void LT__SendSite_ensure_initialized(LT__SendSite* site, char* selector){
    if (site->selector == LT_INVALID){
        site->selector = LT_Symbol_new_in(LT_PACKAGE_KEYWORD, selector);
    }
}

#define LT__CONCAT2(a, b) a##b
#define LT__CONCAT(a, b) LT__CONCAT2(a, b)

#define LT_SEND_ARGS(receiver, selector_name, arguments) \
    ({ \
        static LT__SendSite LT__CONCAT(LT__send_site_, __LINE__) = LT__SEND_SITE_INITIALIZER; \
        LT__SendSite_ensure_initialized(&LT__CONCAT(LT__send_site_, __LINE__), (selector_name)); \
        LT_send((receiver), LT__CONCAT(LT__send_site_, __LINE__).selector, (arguments), NULL); \
    })

#define LT_SEND(receiver, selector_name, ...) \
    LT_SEND_ARGS((receiver), (selector_name), LT_list(__VA_ARGS__ __VA_OPT__(,) LT_INVALID))

#define LT_APPLY(callable, ...) \
    LT_applyv((callable), __VA_ARGS__ __VA_OPT__(,) LT_INVALID)

extern LT_Value LT_super_send(
    LT_Value receiver,
    LT_Value precedence_list,
    LT_Value selector,
    LT_Value arguments,
    LT_TailCallUnwindMarker* tail_call_unwind_marker
);


LT__END_DECLS

#endif