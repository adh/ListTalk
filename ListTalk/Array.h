#ifndef H__ListTalk__Tuple__
#define H__ListTalk__Tuple__

#include <ListTalk/env_macros.h>

#include <ListTalk/decl_macros.h>
#include <ListTalk/OOP.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Array);

extern LT_Array* LT_Array_new(size_t size);
extern size_t LT_Array_length(LT_Array* array);
extern LT_Object* LT_Array_at(LT_Array* array, size_t position);
extern void LT_Array_at_put(
    LT_Array* array, 
    size_t position, 
    LT_Object* object
);

extern LT_Class * const LT_ImmutableTuple_class; typedef struct LT_ImmutableTuple LT_ImmutableTuple;

extern size_t LT_ImmutableTuple_length(LT_ImmutableTuple* tuple);
extern LT_Object* LT_ImmutableTuple_at(
    LT_ImmutableTuple* tuple,
    size_t position
);

LT_DECLARE_CLASS(LT_ImmutableTupleBuilder);

extern void LT_ImmutableTupleBuilder_put(
    LT_ImmutableTupleBuilder* builder, 
    LT_Object* object
);
extern LT_ImmutableTuple* LT_ImmutableTupleBuilder_value(
    LT_ImmutableTupleBuilder* builder
);

LT__END_DECLS

#endif