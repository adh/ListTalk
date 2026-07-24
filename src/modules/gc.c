/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>

#include <gc/gc.h>
#include <stdint.h>

static void bind_gc_primitive(LT_Environment* environment,
                              LT_Package* package,
                              LT_Primitive* primitive){
    LT_Environment_bind(
        environment,
        LT_Symbol_new_in(package, primitive->name),
        LT_Primitive_from_static(primitive),
        LT_ENV_BINDING_FLAG_CONSTANT
    );
}

static LT_Value gc_size_value(size_t value){
    return LT_Integer_from_uintmax((uintmax_t)value);
}

static LT_Value gc_keyword(char* name){
    return LT_Symbol_new_in(LT_PACKAGE_KEYWORD, name);
}

static LT_Value gc_stat_pair(char* name, LT_Value value){
    return LT_cons(gc_keyword(name), value);
}

LT_DEFINE_PRIMITIVE(
    primitive_gc_collect_bang,
    "collect!",
    "()",
    "Force a full garbage collection cycle."
){
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_ARG_END(arguments);
    GC_gcollect();
    return LT_NIL;
}

LT_DEFINE_PRIMITIVE(
    primitive_gc_heap_size,
    "heap-size",
    "()",
    "Return the current heap size in bytes."
){
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_ARG_END(arguments);
    return gc_size_value(GC_get_heap_size());
}

LT_DEFINE_PRIMITIVE(
    primitive_gc_free_bytes,
    "free-bytes",
    "()",
    "Return free bytes in the current heap."
){
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_ARG_END(arguments);
    return gc_size_value(GC_get_free_bytes());
}

LT_DEFINE_PRIMITIVE(
    primitive_gc_unmapped_bytes,
    "unmapped-bytes",
    "()",
    "Return bytes unmapped from the current heap."
){
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_ARG_END(arguments);
    return gc_size_value(GC_get_unmapped_bytes());
}

LT_DEFINE_PRIMITIVE(
    primitive_gc_bytes_since_gc,
    "bytes-since-gc",
    "()",
    "Return bytes allocated since the last garbage collection."
){
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_ARG_END(arguments);
    return gc_size_value(GC_get_bytes_since_gc());
}

LT_DEFINE_PRIMITIVE(
    primitive_gc_total_bytes,
    "total-bytes",
    "()",
    "Return total bytes allocated by the collector."
){
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_ARG_END(arguments);
    return gc_size_value(GC_get_total_bytes());
}

LT_DEFINE_PRIMITIVE(
    primitive_gc_collection_count,
    "collection-count",
    "()",
    "Return the number of completed garbage collections."
){
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_ARG_END(arguments);
    return LT_Integer_from_uintmax((uintmax_t)GC_get_gc_no());
}

LT_DEFINE_PRIMITIVE(
    primitive_gc_statistics,
    "statistics",
    "()",
    "Return an association list of garbage collector statistics."
){
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_ARG_END(arguments);
    return LT_listn(
        6,
        gc_stat_pair("heap-size", gc_size_value(GC_get_heap_size())),
        gc_stat_pair("free-bytes", gc_size_value(GC_get_free_bytes())),
        gc_stat_pair("unmapped-bytes", gc_size_value(GC_get_unmapped_bytes())),
        gc_stat_pair("bytes-since-gc", gc_size_value(GC_get_bytes_since_gc())),
        gc_stat_pair("total-bytes", gc_size_value(GC_get_total_bytes())),
        gc_stat_pair("collection-count", LT_Integer_from_uintmax((uintmax_t)GC_get_gc_no()))
    );
}

void ListTalk_gc_load(LT_Environment* environment){
    LT_Package* package = LT_Package_new("ListTalk-GC");

    bind_gc_primitive(environment, package, &primitive_gc_collect_bang);
    bind_gc_primitive(environment, package, &primitive_gc_heap_size);
    bind_gc_primitive(environment, package, &primitive_gc_free_bytes);
    bind_gc_primitive(environment, package, &primitive_gc_unmapped_bytes);
    bind_gc_primitive(environment, package, &primitive_gc_bytes_since_gc);
    bind_gc_primitive(environment, package, &primitive_gc_total_bytes);
    bind_gc_primitive(environment, package, &primitive_gc_collection_count);
    bind_gc_primitive(environment, package, &primitive_gc_statistics);
    LT_loader_provide(environment, "gc");
}
