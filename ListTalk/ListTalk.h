/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__ListTalk__
#define H__ListTalk__ListTalk__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/macros/arg_macros.h>

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
#include <ListTalk/classes/List.h>
#include <ListTalk/classes/ImmutableList.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/SourceLocation.h>
#include <ListTalk/classes/StackFrame.h>
#include <ListTalk/classes/Vector.h>
#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/Condition.h>
#include <ListTalk/classes/Closure.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/InvocationContextKind.h>
#include <ListTalk/classes/Macro.h>
#include <ListTalk/classes/SpecialForm.h>
#include <ListTalk/classes/IdentityDictionary.h>
#include <ListTalk/classes/Dictionary.h>
#include <ListTalk/classes/Stream.h>
#include <ListTalk/classes/Instant.h>
#include <ListTalk/classes/Duration.h>
#include <ListTalk/classes/DateTime.h>
#include <ListTalk/classes/UTCDateTime.h>

LT__BEGIN_DECLS

typedef struct LT_TailCallUnwindMarker_s LT_TailCallUnwindMarker;

extern void LT_init(void);

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

extern LT_Value LT_send(
    LT_Value receiver,
    LT_Value selector,
    LT_Value arguments,
    LT_TailCallUnwindMarker* tail_call_unwind_marker
);

extern LT_Value LT_super_send(
    LT_Value receiver,
    LT_Value precedence_list,
    LT_Value selector,
    LT_Value arguments,
    LT_TailCallUnwindMarker* tail_call_unwind_marker
);

LT__END_DECLS

#endif
