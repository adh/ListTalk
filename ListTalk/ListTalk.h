/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__ListTalk__
#define H__ListTalk__ListTalk__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/macros/method_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/vm/Environment.h>
#include <ListTalk/vm/compiler.h>
#include <ListTalk/vm/loader.h>
#include <ListTalk/vm/base_env.h>
#include <ListTalk/vm/conditions.h>
#include <ListTalk/classes/Object.h>
#include <ListTalk/classes/Boolean.h>
#include <ListTalk/classes/Nil.h>
#include <ListTalk/classes/Character.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/ComplexNumber.h>
#include <ListTalk/classes/RealNumber.h>
#include <ListTalk/classes/RationalNumber.h>
#include <ListTalk/classes/Integer.h>
#include <ListTalk/classes/InexactComplexNumber.h>
#include <ListTalk/classes/ExactComplexNumber.h>
#include <ListTalk/classes/Float.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/classes/SmallFraction.h>
#include <ListTalk/classes/BigInteger.h>
#include <ListTalk/classes/Fraction.h>
#include <ListTalk/classes/Iterator.h>
#include <ListTalk/classes/List.h>
#include <ListTalk/classes/ImmutableList.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Package.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/classes/SourceLocation.h>
#include <ListTalk/classes/StackFrame.h>
#include <ListTalk/classes/Message.h>
#include <ListTalk/classes/BindingDescriptor.h>
#include <ListTalk/classes/MethodDescriptor.h>
#include <ListTalk/classes/Vector.h>
#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/UUID.h>
#include <ListTalk/classes/Condition.h>
#include <ListTalk/classes/Restart.h>
#include <ListTalk/classes/CompoundForm.h>
#include <ListTalk/classes/Function.h>
#include <ListTalk/classes/Closure.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/InvocationContextKind.h>
#include <ListTalk/classes/Macro.h>
#include <ListTalk/classes/SpecialForm.h>
#include <ListTalk/classes/IdentityDictionary.h>
#include <ListTalk/classes/Dictionary.h>
#include <ListTalk/classes/Set.h>
#include <ListTalk/classes/IdentitySet.h>
#include <ListTalk/classes/WeakIdentitySet.h>
#include <ListTalk/classes/WeakKeyIdentityDictionary.h>
#include <ListTalk/classes/WeakValueIdentityDictionary.h>
#include <ListTalk/classes/WeakReference.h>
#include <ListTalk/classes/DynamicVariable.h>
#include <ListTalk/classes/Stream.h>
#include <ListTalk/classes/Instant.h>
#include <ListTalk/classes/Duration.h>
#include <ListTalk/classes/DateTime.h>
#include <ListTalk/classes/UTCDateTime.h>
#include <ListTalk/classes/Mutex.h>
#include <ListTalk/classes/Thread.h>

LT__BEGIN_DECLS

typedef struct LT_TailCallUnwindMarker_s LT_TailCallUnwindMarker;

extern void LT__init(void);

#define LT_INIT()   \
    do {            \
        GC_INIT();  \
        LT__init(); \
    } while (0)

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
