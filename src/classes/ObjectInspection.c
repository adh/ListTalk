/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Class.h>
#include <ListTalk/classes/List.h>
#include <ListTalk/classes/ObjectInspection.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/macros/arg_macros.h>

#include <stddef.h>
#include <stdint.h>

static LT_Slot_Descriptor ObjectInspection_slots[] = {
    {"name", offsetof(LT_ObjectInspection, name), &LT_SlotType_ReadonlyObject},
    {"description", offsetof(LT_ObjectInspection, description), &LT_SlotType_ReadonlyObject},
    {"slots", offsetof(LT_ObjectInspection, slots), &LT_SlotType_ReadonlyObject},
    {"contents-label", offsetof(LT_ObjectInspection, contents_label), &LT_SlotType_ReadonlyObject},
    {"contents", offsetof(LT_ObjectInspection, contents), &LT_SlotType_ReadonlyObject},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static int plist_p(LT_Value value){
    while (value != LT_NIL){
        if (!LT_Pair_p(value)){
            return 0;
        }
        value = LT_cdr(value);
        if (!LT_Pair_p(value)){
            return 0;
        }
        value = LT_cdr(value);
    }
    return 1;
}

static void ObjectInspection_debugPrintOn(LT_Value object, FILE* stream){
    LT_ObjectInspection* inspection = LT_ObjectInspection_from_value(object);

    fputs("#<ObjectInspection ", stream);
    LT_Value_debugPrintOn(inspection->name, stream);
    fputc('>', stream);
}

#define DEFINE_OBJECT_INSPECTION_ACCESSOR(method_name, selector, accessor, text) \
    LT_DEFINE_PRIMITIVE( \
        method_name, \
        "ObjectInspection>>" selector, \
        "(self)", \
        text \
    ){ \
        LT_Value cursor = arguments; \
        LT_Value self; \
        (void)tail_call_unwind_marker; \
        LT_OBJECT_ARG(cursor, self); \
        LT_ARG_END(cursor); \
        return accessor(LT_ObjectInspection_from_value(self)); \
    }

DEFINE_OBJECT_INSPECTION_ACCESSOR(
    object_inspection_method_name,
    "name",
    LT_ObjectInspection_name,
    "Return inspection name."
)
DEFINE_OBJECT_INSPECTION_ACCESSOR(
    object_inspection_method_description,
    "description",
    LT_ObjectInspection_description,
    "Return inspection description."
)
DEFINE_OBJECT_INSPECTION_ACCESSOR(
    object_inspection_method_slots,
    "slots",
    LT_ObjectInspection_slots,
    "Return inspection slot property list."
)
DEFINE_OBJECT_INSPECTION_ACCESSOR(
    object_inspection_method_contents_label,
    "contents-label",
    LT_ObjectInspection_contents_label,
    "Return inspection contents heading."
)
DEFINE_OBJECT_INSPECTION_ACCESSOR(
    object_inspection_method_contents,
    "contents",
    LT_ObjectInspection_contents,
    "Return inspection contents property list."
)

#undef DEFINE_OBJECT_INSPECTION_ACCESSOR

LT_DEFINE_PRIMITIVE(
    object_inspection_class_method_new,
    "ObjectInspection class>>newName:description:slots:contentsLabel:contents:",
    "(self name description slots contents-label contents)",
    "Return an object inspection with the supplied fields."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value name;
    LT_Value description;
    LT_Value slots;
    LT_Value contents_label;
    LT_Value contents;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, name);
    LT_OBJECT_ARG(cursor, description);
    LT_OBJECT_ARG(cursor, slots);
    LT_OBJECT_ARG(cursor, contents_label);
    LT_OBJECT_ARG(cursor, contents);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)&LT_ObjectInspection_class){
        LT_error(
            "newName:description:slots:contentsLabel:contents: class method "
            "is only supported on ObjectInspection"
        );
    }
    return LT_ObjectInspection_new(
        name,
        description,
        slots,
        contents_label,
        contents
    );
}

static LT_Method_Descriptor ObjectInspection_methods[] = {
    {"name", &object_inspection_method_name},
    {"description", &object_inspection_method_description},
    {"slots", &object_inspection_method_slots},
    {"contents-label", &object_inspection_method_contents_label},
    {"contents", &object_inspection_method_contents},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor ObjectInspection_class_methods[] = {
    {
        "newName:description:slots:contentsLabel:contents:",
        &object_inspection_class_method_new
    },
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_ObjectInspection) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "ObjectInspection",
    .documentation = "Immutable, presentation-independent description of an object.",
    .instance_size = sizeof(LT_ObjectInspection),
    .class_flags = LT_CLASS_FLAG_FINAL | LT_CLASS_FLAG_IMMUTABLE,
    .debugPrintOn = ObjectInspection_debugPrintOn,
    .slots = ObjectInspection_slots,
    .methods = ObjectInspection_methods,
    .class_methods = ObjectInspection_class_methods,
};

LT_Value LT_ObjectInspection_new(LT_Value name,
                                 LT_Value description,
                                 LT_Value slots,
                                 LT_Value contents_label,
                                 LT_Value contents){
    LT_ObjectInspection* inspection;

    if (!LT_String_p(name)){
        LT_type_error(name, &LT_String_class);
    }
    if (!LT_String_p(description)){
        LT_type_error(description, &LT_String_class);
    }
    if (!plist_p(slots)){
        LT_error("ObjectInspection slots must be a property list");
    }
    if (!LT_String_p(contents_label)){
        LT_type_error(contents_label, &LT_String_class);
    }
    if (!plist_p(contents)){
        LT_error("ObjectInspection contents must be a property list");
    }

    inspection = LT_Class_ALLOC(LT_ObjectInspection);
    inspection->name = name;
    inspection->description = description;
    inspection->slots = slots;
    inspection->contents_label = contents_label;
    inspection->contents = contents;
    return (LT_Value)(uintptr_t)inspection;
}

LT_Value LT_ObjectInspection_name(LT_ObjectInspection* inspection){
    return inspection->name;
}

LT_Value LT_ObjectInspection_description(LT_ObjectInspection* inspection){
    return inspection->description;
}

LT_Value LT_ObjectInspection_slots(LT_ObjectInspection* inspection){
    return inspection->slots;
}

LT_Value LT_ObjectInspection_contents_label(LT_ObjectInspection* inspection){
    return inspection->contents_label;
}

LT_Value LT_ObjectInspection_contents(LT_ObjectInspection* inspection){
    return inspection->contents;
}
