/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/vm/stack_trace.h>
#include <ListTalk/classes/StackFrame.h>
#include <ListTalk/vm/value.h>
#include <ListTalk/utils.h>

#include <stdio.h>

_Thread_local LT_StackFrame* LT__stack_trace_stack = NULL;

unsigned int LT_stack_trace_depth(void){
    unsigned int depth = 0;
    LT_StackFrame* frame = LT__stack_trace_stack;

    while (frame != NULL){
        depth += 1;
        frame = frame->previous;
    }

    return depth;
}

LT_Value LT_stack_trace_capture(void){
    LT_StackFrame* frame = LT__stack_trace_stack;
    LT_ListBuilder* builder = LT_ListBuilder_new();

    while (frame != NULL){
        LT_Value snapshot_frame = LT_NIL;

        if (frame->type == LT_STACK_FRAME_TYPE_EVAL){
            snapshot_frame = (LT_Value)(uintptr_t)LT_EvalStackFrame_new(
                frame->arguments.eval.expression,
                frame->arguments.eval.environment
            );
        } else if (frame->type == LT_STACK_FRAME_TYPE_APPLY){
            snapshot_frame = (LT_Value)(uintptr_t)LT_ApplyStackFrame_new(
                frame->arguments.apply.callable,
                frame->arguments.apply.arguments
            );
        }

        if (snapshot_frame != LT_NIL){
            LT_ListBuilder_append(builder, snapshot_frame);
        }
        frame = frame->previous;
    }

    return LT_ListBuilder_value(builder);
}

void LT_stack_trace_print(FILE* stream){
    LT_Value snapshot = LT_stack_trace_capture();
    LT_Value cursor = snapshot;
    unsigned int index = 0;

    if (snapshot == LT_NIL){
        return;
    }

    fputs("Backtrace:\n", stream);
    while (cursor != LT_NIL){
        LT_Value frame_object = LT_car(cursor);

        fprintf(stream, "  #%u ", index);
        if (LT_EvalStackFrame_p(frame_object)){
            LT_EvalStackFrame* frame = LT_EvalStackFrame_from_value(frame_object);
            fputs("eval expr=", stream);
            LT_Value_debugPrintOn(LT_EvalStackFrame_expression(frame), stream);
            fputs(" env=", stream);
            LT_Value_debugPrintOn(
                (LT_Value)(uintptr_t)LT_EvalStackFrame_environment(frame),
                stream
            );
            fputc('\n', stream);
        } else if (LT_ApplyStackFrame_p(frame_object)){
            LT_ApplyStackFrame* frame = LT_ApplyStackFrame_from_value(frame_object);
            fputs("apply callable=", stream);
            LT_Value_debugPrintOn(LT_ApplyStackFrame_callable(frame), stream);
            fputs(" args=", stream);
            LT_Value_debugPrintOn(LT_ApplyStackFrame_arguments(frame), stream);
            fputc('\n', stream);
        } else {
            fputs("unknown-frame\n", stream);
        }
        index += 1;
        cursor = LT_cdr(cursor);
    }
}
