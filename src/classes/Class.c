/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/classes/Class.h>
#include <ListTalk/ListTalk.h>
#include <ListTalk/vm/epoch.h>
#include <ListTalk/classes/Closure.h>
#include <ListTalk/classes/IdentitySet.h>
#include <ListTalk/classes/ImmutableList.h>
#include <ListTalk/classes/IdentityDictionary.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/MethodDescriptor.h>
#include <ListTalk/classes/Package.h>
#include <ListTalk/classes/Set.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/utils.h>

#include <stdatomic.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

static LT_Value object_slot_ref(LT_Class_Slot* slot, LT_Value object){
    LT_Value* val = (LT_Value*)(
        (uint8_t*)LT_VALUE_POINTER_VALUE(object) + slot->offset
    );
    return *val;
}
static LT_Value atomic_object_slot_ref(LT_Class_Slot* slot, LT_Value object){
    _Atomic(LT_Value)* val = (_Atomic(LT_Value)*)(
        (uint8_t*)LT_VALUE_POINTER_VALUE(object) + slot->offset
    );
    return atomic_load_explicit(val, memory_order_acquire);
}
static LT_Value readonly_native_object_pointer_slot_ref(
    LT_Class_Slot* slot,
    LT_Value object
){
    void** pointer = (void**)(
        (uint8_t*)LT_VALUE_POINTER_VALUE(object) + slot->offset
    );

    return *pointer == NULL ? LT_NIL : (LT_Value)(uintptr_t)*pointer;
}
static void object_slot_set(LT_Class_Slot* slot, LT_Value object, LT_Value value){
    LT_Value* val = (LT_Value*)(
        (uint8_t*)LT_VALUE_POINTER_VALUE(object) + slot->offset
    );
    *val = value;
}
static void readonly_object_slot_set(LT_Class_Slot* slot,
                                     LT_Value object,
                                     LT_Value value){
    (void)slot;
    (void)object;
    (void)value;
    LT_error("Readonly slot");
}

LT_SlotType LT_SlotType_Object = {
    .ref = object_slot_ref,
    .set = object_slot_set,
};
LT_SlotType LT_SlotType_ReadonlyObject = {
    .ref = object_slot_ref,
    .set = readonly_object_slot_set,
};
LT_SlotType LT_SlotType_ReadonlyAtomicObject = {
    .ref = atomic_object_slot_ref,
    .set = readonly_object_slot_set,
};
LT_SlotType LT_SlotType_ReadonlyNativeObjectPointer = {
    .ref = readonly_native_object_pointer_slot_ref,
    .set = readonly_object_slot_set,
};

static void invalidate_inline_caches(void){
    LT_ilc_epoch_increment();
}

static size_t Class_default_hash(LT_Value obj){
    return LT_pointer_hash((void*)(uintptr_t)obj);
}

static int Class_default_equal_p(LT_Value left, LT_Value right){
    return left == right;
}

static void Class_debugPrintOn(LT_Value obj, FILE* stream){
    LT_Class* klass = (LT_Class*)LT_VALUE_POINTER_VALUE(obj);

    if (klass == &LT_Class_class_class){
        fputs("#<Class Metaclass>", stream);
        return;
    }
    if (LT_Symbol_p(klass->name)){
        fputs("#<Class ", stream);
        fputs(
            LT_Symbol_name(LT_Symbol_from_value(klass->name)),
            stream
        );
        fputc('>', stream);
        return;
    }

    fprintf(stream, "#<Class 0x%" PRIxPTR ">", (uintptr_t)klass);
}

static LT_Slot_Descriptor Class_slots[] = {
    {"name", offsetof(LT_Class, name), &LT_SlotType_Object},
    {"methods", offsetof(LT_Class, methods), &LT_SlotType_Object},
    {"method-cache", offsetof(LT_Class, method_cache), &LT_SlotType_Object},
    {"documentation", offsetof(LT_Class, documentation), &LT_SlotType_Object},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

LT_DECLARE_PRIMITIVE(
    class_method_slots,
    "Class>>slots",
    "(self)",
    "Return class slot names."
);
LT_DECLARE_PRIMITIVE(
    class_method_documentation,
    "Class>>documentation",
    "(self)",
    "Return class documentation."
);
LT_DECLARE_PRIMITIVE(
    class_method_precedence_list,
    "Class>>precedenceList",
    "(self)",
    "Return class precedence list."
);
LT_DECLARE_PRIMITIVE(
    class_method_binary_lookup_selector,
    "Class>>>>",
    "(self selector)",
    "Return direct method for selector or nil."
);
LT_DECLARE_PRIMITIVE(
    class_method_lookup_selector,
    "Class>>lookupSelector:",
    "(self selector)",
    "Return resolved method for selector or nil."
);
LT_DECLARE_PRIMITIVE(
    class_method_selectors,
    "Class>>selectors",
    "(self)",
    "Return direct method selectors."
);
LT_DECLARE_PRIMITIVE(
    class_method_all_selectors,
    "Class>>allSelectors",
    "(self)",
    "Return direct and inherited method selectors."
);
LT_DECLARE_PRIMITIVE(
    class_method_selectors_as_list,
    "Class>>selectorsAsList",
    "(self)",
    "Return direct method selectors as a list."
);
LT_DECLARE_PRIMITIVE(
    class_method_all_selectors_as_list,
    "Class>>allSelectorsAsList",
    "(self)",
    "Return direct and inherited method selectors as a list."
);
LT_DECLARE_PRIMITIVE(
    class_method_methods_do,
    "Class>>methodsDo:",
    "(self callable)",
    "Call callable for each direct method descriptor."
);
LT_DECLARE_PRIMITIVE(
    class_method_methods_as_list,
    "Class>>methodsAsList",
    "(self)",
    "Return direct method descriptors as a list."
);
LT_DECLARE_PRIMITIVE(
    class_method_inspection,
    "Class>>inspection",
    "(self)",
    "Return an inspection whose contents are direct methods."
);
LT_DECLARE_PRIMITIVE(
    class_method_all_methods_do,
    "Class>>allMethodsDo:",
    "(self callable)",
    "Call callable for each most-specific method descriptor."
);
LT_DECLARE_PRIMITIVE(
    class_method_all_methods_as_list,
    "Class>>allMethodsAsList",
    "(self)",
    "Return most-specific direct and inherited method descriptors as a list."
);
LT_DECLARE_PRIMITIVE(
    class_method_add_method_with_selector,
    "Class>>addMethod:withSelector:",
    "(self method selector)",
    "Add direct method for selector."
);
LT_DECLARE_PRIMITIVE(
    class_method_alloc,
    "Class>>alloc",
    "(self)",
    "Allocate empty instance for allocatable class."
);

static LT_Method_Descriptor Class_methods[] = {
    {"slots", &class_method_slots},
    {"documentation", &class_method_documentation},
    {"precedenceList", &class_method_precedence_list},
    {">>", &class_method_binary_lookup_selector},
    {"lookupSelector:", &class_method_lookup_selector},
    {"selectors", &class_method_selectors},
    {"allSelectors", &class_method_all_selectors},
    {"selectorsAsList", &class_method_selectors_as_list},
    {"allSelectorsAsList", &class_method_all_selectors_as_list},
    {"methodsDo:", &class_method_methods_do},
    {"methodsAsList", &class_method_methods_as_list},
    {"inspection", &class_method_inspection},
    {"allMethodsDo:", &class_method_all_methods_do},
    {"allMethodsAsList", &class_method_all_methods_as_list},
    {"addMethod:withSelector:", &class_method_add_method_with_selector},
    {"alloc", &class_method_alloc},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Class) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Class",
    .documentation = "Runtime representation of classes and metaclasses.",
    .instance_size = sizeof(LT_Class),
    .class_flags = LT_CLASS_FLAG_ABSTRACT,
    .debugPrintOn = Class_debugPrintOn,
    .slots = Class_slots,
    .methods = Class_methods,
};

static LT_Class** make_single_superclass_list(LT_Class* superclass){
    LT_Class** superclasses;

    if (superclass == NULL){
        superclasses = GC_MALLOC(sizeof(LT_Class*));
        superclasses[0] = NULL;
        return superclasses;
    }

    superclasses = GC_MALLOC(sizeof(LT_Class*) * 2);
    superclasses[0] = superclass;
    superclasses[1] = NULL;
    return superclasses;
}

static LT_Class** make_native_superclass_list(LT_Class_Descriptor* descriptor){
    size_t mixin_count = 0;
    size_t i;
    LT_Class** superclasses;

    if (descriptor->mixins != NULL){
        while (descriptor->mixins[mixin_count] != NULL){
            mixin_count++;
        }
    }

    superclasses = GC_MALLOC(
        sizeof(LT_Class*) * (mixin_count + (descriptor->superclass != NULL) + 1)
    );
    for (i = 0; i < mixin_count; i++){
        superclasses[i] = descriptor->mixins[i];
    }
    if (descriptor->superclass != NULL){
        superclasses[mixin_count++] = descriptor->superclass;
    }
    superclasses[mixin_count] = NULL;
    return superclasses;
}

static LT_Value precedence_list_storage(LT_Value precedence_list){
    return (LT_Value)(uintptr_t)LT_VALUE_POINTER_VALUE(precedence_list);
}

static LT_Value make_single_inheritance_precedence_list(LT_Class* klass,
                                                        LT_Class* superclass){
    size_t superclass_length = 0;
    LT_Value* values;
    LT_Value precedence_list;
    size_t i;
    LT_Value superclass_precedence = LT_NIL;

    if (superclass != NULL && LT_Class_precedence_list(superclass) != LT_NIL){
        superclass_precedence = LT_Class_precedence_list(superclass);
        while (superclass_precedence != LT_NIL){
            superclass_length++;
            superclass_precedence = LT_ImmutableList_cdr(superclass_precedence);
        }
    }

    values = GC_MALLOC(sizeof(LT_Value) * (superclass_length + 1));
    values[0] = (LT_Value)(uintptr_t)klass;
    superclass_precedence = (superclass != NULL)
        ? LT_Class_precedence_list(superclass)
        : LT_NIL;
    for (i = 0; i < superclass_length; i++){
        values[i + 1] = LT_ImmutableList_car(superclass_precedence);
        superclass_precedence = LT_ImmutableList_cdr(superclass_precedence);
    }
    precedence_list = LT_ImmutableList_new(superclass_length + 1, values);
    return precedence_list_storage(precedence_list);
}

static LT_Value make_precedence_list(LT_Class* self);

#define LT_EARLY_NATIVE_CLASS_CAPACITY 64

static int core_class_cycle_finalized = 0;
static LT_Class* early_native_classes[LT_EARLY_NATIVE_CLASS_CAPACITY];
static size_t early_native_class_count = 0;

static int class_basics_materialized_p(LT_Class* klass){
    return klass->native_descriptor == NULL
        && klass->methods != LT_INVALID
        && klass->method_cache != LT_INVALID
        && klass->name != LT_INVALID;
}

static void record_early_native_class(LT_Class* klass){
    size_t i;

    if (klass == &LT_Object_class || klass == &LT_Class_class){
        return;
    }

    for (i = 0; i < early_native_class_count; i++){
        if (early_native_classes[i] == klass){
            return;
        }
    }
    if (early_native_class_count >= LT_EARLY_NATIVE_CLASS_CAPACITY){
        LT_error("Too many early native classes");
    }

    early_native_classes[early_native_class_count++] = klass;
}

static void refresh_native_class_topology(LT_Class* klass){
    LT_Class* metaclass;
    LT_Class* metaclass_superclass = NULL;

    invalidate_inline_caches();
    klass->precedence_list = make_precedence_list(klass);

    metaclass = klass->base.klass;
    if (metaclass == NULL){
        return;
    }

    if (metaclass->superclasses != NULL){
        metaclass_superclass = metaclass->superclasses[0];
    }
    metaclass->precedence_list = make_single_inheritance_precedence_list(
        metaclass,
        metaclass_superclass
    );
    if (metaclass_superclass != NULL){
        metaclass->slot_count = metaclass_superclass->slot_count;
        metaclass->slots = metaclass_superclass->slots;
        metaclass->hash = metaclass_superclass->hash;
        metaclass->equal_p = metaclass_superclass->equal_p;
    }
}

static void refresh_early_native_class_topology(void){
    size_t i;

    for (i = 0; i < early_native_class_count; i++){
        refresh_native_class_topology(early_native_classes[i]);
    }
    early_native_class_count = 0;
}

static void finalize_core_class_cycle_if_ready(void){
    if (core_class_cycle_finalized){
        return;
    }
    if (!class_basics_materialized_p(&LT_Object_class)
        || !class_basics_materialized_p(&LT_Class_class)){
        return;
    }

    LT_Object_class.superclasses = make_single_superclass_list(NULL);
    invalidate_inline_caches();
    LT_Object_class.precedence_list = make_single_inheritance_precedence_list(
        &LT_Object_class,
        NULL
    );

    LT_Class_class.superclasses = make_single_superclass_list(&LT_Object_class);
    LT_Class_class.precedence_list = make_single_inheritance_precedence_list(
        &LT_Class_class,
        &LT_Object_class
    );

    LT_Class_class_class.superclasses = make_single_superclass_list(&LT_Class_class);
    LT_Class_class_class.precedence_list = make_single_inheritance_precedence_list(
        &LT_Class_class_class,
        &LT_Class_class
    );
    LT_Class_class_class.slot_count = LT_Class_class.slot_count;
    LT_Class_class_class.slots = LT_Class_class.slots;
    LT_Class_class_class.hash = LT_Class_class.hash;
    LT_Class_class_class.equal_p = LT_Class_class.equal_p;

    LT_Object_class_class.superclasses = make_single_superclass_list(&LT_Class_class);
    LT_Object_class_class.precedence_list = make_single_inheritance_precedence_list(
        &LT_Object_class_class,
        &LT_Class_class
    );
    LT_Object_class_class.slot_count = LT_Class_class.slot_count;
    LT_Object_class_class.slots = LT_Class_class.slots;
    LT_Object_class_class.hash = LT_Class_class.hash;
    LT_Object_class_class.equal_p = LT_Class_class.equal_p;

    core_class_cycle_finalized = 1;
    refresh_early_native_class_topology();
}

static size_t count_slot_descriptors(LT_Slot_Descriptor* descriptor_slots){
    size_t count = 0;

    if (descriptor_slots == NULL){
        return 0;
    }
    while (descriptor_slots[count].name != NULL){
        count++;
    }
    return count;
}

static void materialize_direct_methods(
    LT_Class* klass,
    LT_Method_Descriptor* descriptor_methods
){
    size_t i = 0;

    if (descriptor_methods == NULL){
        return;
    }

    while (descriptor_methods[i].selector != NULL){
        LT_Class_addMethod(
            klass,
            LT_Symbol_new_in(LT_PACKAGE_KEYWORD, descriptor_methods[i].selector),
            LT_Primitive_from_static(descriptor_methods[i].primitive)
        );
        i++;
    }
}

static LT_Value materialize_documentation(LT_Class* klass, char* documentation){
    if (documentation == NULL){
        return LT_NIL;
    }
    if (klass != &LT_String_class){
        LT_init_native_class(&LT_String_class);
    }
    return (LT_Value)(uintptr_t)LT_String_new_cstr(documentation);
}

static LT_Value make_metaclass_name(LT_Value class_name){
    char* name;
    size_t length;
    char* metaclass_name;
    LT_Package* package;

    if (!LT_Symbol_p(class_name)){
        return LT_NIL;
    }

    name = LT_Symbol_name(LT_Symbol_from_value(class_name));
    package = LT_Symbol_package(LT_Symbol_from_value(class_name));
    length = strlen(name);
    metaclass_name = GC_MALLOC_ATOMIC(length + strlen(" class") + 1);
    memcpy(metaclass_name, name, length);
    memcpy(metaclass_name + length, " class", strlen(" class") + 1);
    return package == NULL
        ? LT_Symbol_new(metaclass_name)
        : LT_Symbol_new_in(package, metaclass_name);
}

static LT_Value native_class_name_symbol(LT_Class_Descriptor* descriptor){
    LT_Package* package;

    if (descriptor->name == NULL){
        return LT_NIL;
    }
    package = descriptor->package == NULL
        ? LT_PACKAGE_LISTTALK
        : LT_Package_new(descriptor->package);
    return LT_Symbol_new_in(package, descriptor->name);
}

static int compare_slots_by_name(const void* left, const void* right){
    const LT_Class_Slot* left_slot = (const LT_Class_Slot*)left;
    const LT_Class_Slot* right_slot = (const LT_Class_Slot*)right;

    if (left_slot->name < right_slot->name){
        return -1;
    }
    if (left_slot->name > right_slot->name){
        return 1;
    }
    return 0;
}

static void materialize_slots(LT_Class* klass,
                              LT_Class* structural_superclass,
                              LT_Slot_Descriptor* descriptor_slots){
    size_t superclass_slot_count = 0;
    size_t descriptor_slot_count;
    size_t max_slot_count;
    size_t slot_count = 0;
    LT_Class_Slot* slots;
    size_t i;

    if (structural_superclass != NULL){
        superclass_slot_count = structural_superclass->slot_count;
    }
    descriptor_slot_count = count_slot_descriptors(descriptor_slots);
    max_slot_count = superclass_slot_count + descriptor_slot_count;

    if (max_slot_count == 0){
        klass->slot_count = 0;
        klass->slots = NULL;
        return;
    }

    slots = GC_MALLOC(sizeof(LT_Class_Slot) * max_slot_count);
    if (superclass_slot_count != 0){
        memcpy(
            slots,
            structural_superclass->slots,
            sizeof(LT_Class_Slot) * superclass_slot_count
        );
        slot_count = superclass_slot_count;
    }

    for (i = 0; i < descriptor_slot_count; i++){
        LT_Value slot_name = LT_Symbol_new(descriptor_slots[i].name);
        LT_Class_Slot slot = {
            .name = slot_name,
            .offset = descriptor_slots[i].offset,
            .type = descriptor_slots[i].type,
        };
        size_t j;
        int replaced = 0;

        if (slot.type == NULL){
            slot.type = &LT_SlotType_Object;
        }

        for (j = 0; j < slot_count; j++){
            if (slots[j].name == slot_name){
                slots[j] = slot;
                replaced = 1;
                break;
            }
        }

        if (!replaced){
            slots[slot_count] = slot;
            slot_count++;
        }
    }

    klass->slot_count = slot_count;
    klass->slots = slots;
    if (slot_count > 1){
        qsort(
            klass->slots,
            klass->slot_count,
            sizeof(LT_Class_Slot),
            compare_slots_by_name
        );
    }
}

static LT_Class** materialize_superclass_array(LT_Value superclasses_value,
                                               size_t* count_out){
    LT_Value cursor = superclasses_value;
    size_t count = 0;
    LT_Class** superclasses;
    size_t i;

    while (cursor != LT_NIL){
        if (!LT_Pair_p(cursor)){
            LT_error("Superclass list must be proper list");
        }
        count++;
        cursor = LT_cdr(cursor);
    }

    superclasses = GC_MALLOC(sizeof(LT_Class*) * (count + 1));
    cursor = superclasses_value;
    for (i = 0; i < count; i++){
        LT_Value superclass_value = LT_car(cursor);
        LT_Class* superclass = LT_Class_from_object(superclass_value);

        LT_init_native_class(superclass);
        superclasses[i] = superclass;
        cursor = LT_cdr(cursor);
    }
    superclasses[count] = NULL;
    *count_out = count;
    return superclasses;
}

static LT_Class** materialize_metaclass_superclass_array(LT_Class** superclasses,
                                                         size_t count){
    LT_Class** metaclass_superclasses;
    size_t i;

    if (count == 0){
        metaclass_superclasses = GC_MALLOC(sizeof(LT_Class*) * 2);
        metaclass_superclasses[0] = &LT_Class_class;
        metaclass_superclasses[1] = NULL;
        return metaclass_superclasses;
    }

    metaclass_superclasses = GC_MALLOC(sizeof(LT_Class*) * (count + 1));
    for (i = 0; i < count; i++){
        metaclass_superclasses[i] = superclasses[i]->base.klass;
    }
    metaclass_superclasses[count] = NULL;
    return metaclass_superclasses;
}

static size_t count_reachable_classes(LT_Class* klass, LT_InlineHash* seen){
    size_t count = 1;
    size_t i;

    LT_PointerHash_at_put(seen, klass, (void*)1);
    if (klass->superclasses == NULL){
        return count;
    }
    for (i = 0; klass->superclasses[i] != NULL; i++){
        LT_Class* superclass = klass->superclasses[i];

        if (!LT_PointerHash_at(seen, superclass)){
            count += count_reachable_classes(superclass, seen);
        }
    }
    return count;
}

static size_t class_array_index(LT_Class** classes,
                                size_t count,
                                LT_Class* klass){
    size_t i;

    for (i = 0; i < count; i++){
        if (classes[i] == klass){
            return i;
        }
    }
    LT_error("Class missing from precedence graph");
}

static void add_precedence_edge(unsigned char* edges,
                                size_t* indegrees,
                                size_t class_count,
                                size_t before,
                                size_t after){
    size_t edge_index = before * class_count + after;

    if (!edges[edge_index]){
        edges[edge_index] = 1;
        indegrees[after]++;
    }
}

static LT_Value make_precedence_list(LT_Class* self){
    LT_InlineHash seen;
    size_t class_count;
    LT_Class** classes;
    unsigned char* edges;
    size_t* indegrees;
    unsigned char* emitted;
    LT_Value* values;
    size_t discovered = 1;
    size_t i;
    size_t output_index;

    LT_InlineHash_init(&seen);
    class_count = count_reachable_classes(self, &seen);
    classes = GC_MALLOC(sizeof(LT_Class*) * class_count);
    classes[0] = self;

    LT_InlineHash_init(&seen);
    LT_PointerHash_at_put(&seen, self, (void*)1);
    for (i = 0; i < discovered; i++){
        size_t j;

        if (classes[i]->superclasses == NULL){
            continue;
        }
        for (j = 0; classes[i]->superclasses[j] != NULL; j++){
            LT_Class* superclass = classes[i]->superclasses[j];

            if (!LT_PointerHash_at(&seen, superclass)){
                LT_PointerHash_at_put(&seen, superclass, (void*)1);
                classes[discovered++] = superclass;
            }
        }
    }

    edges = GC_MALLOC_ATOMIC(class_count * class_count);
    indegrees = GC_MALLOC_ATOMIC(sizeof(size_t) * class_count);
    emitted = GC_MALLOC_ATOMIC(class_count);
    memset(edges, 0, class_count * class_count);
    memset(indegrees, 0, sizeof(size_t) * class_count);
    memset(emitted, 0, class_count);

    for (i = 0; i < class_count; i++){
        LT_Class** superclasses = classes[i]->superclasses;
        size_t j;

        if (superclasses == NULL){
            continue;
        }
        for (j = 0; superclasses[j] != NULL; j++){
            add_precedence_edge(
                edges,
                indegrees,
                class_count,
                i,
                class_array_index(classes, class_count, superclasses[j])
            );
            if (superclasses[j + 1] != NULL){
                add_precedence_edge(
                    edges,
                    indegrees,
                    class_count,
                    class_array_index(classes, class_count, superclasses[j]),
                    class_array_index(classes, class_count, superclasses[j + 1])
                );
            }
        }
    }

    values = GC_MALLOC(sizeof(LT_Value) * class_count);
    for (output_index = 0; output_index < class_count; output_index++){
        size_t selected = class_count;
        size_t j;

        for (i = 0; i < class_count; i++){
            if (!emitted[i] && indegrees[i] == 0){
                selected = i;
                break;
            }
        }
        if (selected == class_count){
            LT_error("Inconsistent class precedence graph");
        }

        emitted[selected] = 1;
        values[output_index] = (LT_Value)(uintptr_t)classes[selected];
        for (j = 0; j < class_count; j++){
            if (edges[selected * class_count + j]){
                indegrees[j]--;
            }
        }
    }

    return precedence_list_storage(LT_ImmutableList_new(class_count, values));
}

static int class_in_precedence_list(LT_Class* klass, LT_Class* candidate){
    LT_Value cursor = LT_Class_precedence_list(klass);

    while (cursor != LT_NIL){
        if (LT_ImmutableList_car(cursor) == (LT_Value)(uintptr_t)candidate){
            return 1;
        }
        cursor = LT_ImmutableList_cdr(cursor);
    }
    return 0;
}

static LT_Class* native_slot_layout_class(LT_Class* klass){
    LT_Value cursor = LT_Class_precedence_list(klass);

    while (cursor != LT_NIL){
        LT_Class* candidate = (LT_Class*)(uintptr_t)LT_ImmutableList_car(cursor);

        if ((candidate->class_flags & LT_CLASS_FLAG_ALLOCATABLE) == 0
            && (candidate->slot_count != 0
                || candidate->instance_size > sizeof(LT_Object))){
            return candidate;
        }
        cursor = LT_ImmutableList_cdr(cursor);
    }
    return NULL;
}

static size_t align_offset(size_t offset, size_t alignment){
    size_t remainder = offset % alignment;
    return (remainder == 0) ? offset : offset + alignment - remainder;
}

static int slot_name_present(LT_Class_Slot* slots,
                             size_t slot_count,
                             LT_Value name){
    size_t i;

    for (i = 0; i < slot_count; i++){
        if (slots[i].name == name){
            return 1;
        }
    }
    return 0;
}

static void materialize_dynamic_slots(LT_Class* klass,
                                      LT_Class** superclasses,
                                      LT_Value slot_names){
    LT_Class* native_layout = NULL;
    size_t inherited_slot_count = 0;
    size_t slot_name_count = 0;
    LT_Value cursor = slot_names;
    LT_Class_Slot* slots;
    size_t slot_count = 0;
    size_t i;
    size_t next_offset;

    for (i = 0; superclasses[i] != NULL; i++){
        LT_Class* candidate = native_slot_layout_class(superclasses[i]);

        inherited_slot_count += superclasses[i]->slot_count;
        if (candidate == NULL || candidate == native_layout){
            continue;
        }
        if (native_layout == NULL
            || class_in_precedence_list(candidate, native_layout)){
            native_layout = candidate;
        } else if (!class_in_precedence_list(native_layout, candidate)){
            LT_error("Multiple inheritance with unrelated native slots is not supported");
        }
    }

    while (cursor != LT_NIL){
        if (!LT_Pair_p(cursor)){
            LT_error("Slot name list must be proper list");
        }
        if (!LT_Symbol_p(LT_car(cursor))){
            LT_type_error(LT_car(cursor), &LT_Symbol_class);
        }
        slot_name_count++;
        cursor = LT_cdr(cursor);
    }

    if (inherited_slot_count + slot_name_count == 0 && native_layout == NULL){
        klass->slot_count = 0;
        klass->slots = NULL;
        klass->instance_size = sizeof(LT_Object);
        return;
    }

    slots = GC_MALLOC(sizeof(LT_Class_Slot) * (inherited_slot_count + slot_name_count));
    if (native_layout != NULL && native_layout->slot_count != 0){
        memcpy(
            slots,
            native_layout->slots,
            sizeof(LT_Class_Slot) * native_layout->slot_count
        );
        slot_count = native_layout->slot_count;
    }

    next_offset = (native_layout != NULL)
        ? native_layout->instance_size
        : sizeof(LT_Object);
    next_offset = align_offset(next_offset, _Alignof(LT_Value));

    for (i = 0; superclasses[i] != NULL; i++){
        size_t j;

        for (j = 0; j < superclasses[i]->slot_count; j++){
            LT_Class_Slot inherited_slot = superclasses[i]->slots[j];

            if (slot_name_present(slots, slot_count, inherited_slot.name)){
                continue;
            }
            slots[slot_count] = inherited_slot;
            slots[slot_count].offset = next_offset;
            slot_count++;
            next_offset += sizeof(LT_Value);
        }
    }

    cursor = slot_names;
    while (cursor != LT_NIL){
        LT_Value slot_name = LT_car(cursor);

        if (!slot_name_present(slots, slot_count, slot_name)){
            slots[slot_count].name = slot_name;
            slots[slot_count].offset = next_offset;
            slots[slot_count].type = &LT_SlotType_Object;
            slot_count++;
            next_offset += sizeof(LT_Value);
        }
        cursor = LT_cdr(cursor);
    }

    klass->slots = slots;
    klass->slot_count = slot_count;
    klass->instance_size = next_offset;
    if (slot_count > 1){
        qsort(
            klass->slots,
            klass->slot_count,
            sizeof(LT_Class_Slot),
            compare_slots_by_name
        );
    }
}

void LT_init_native_class(LT_Class* klass){
    LT_Class_Descriptor* descriptor = klass->native_descriptor;
    LT_Class* metaclass;
    size_t i;

    if (descriptor == NULL){
        return;
    }

    /* Mark as in-progress before recursive initialization to break cycles. */
    klass->native_descriptor = NULL;

    if (descriptor->superclass != NULL){
        LT_init_native_class(descriptor->superclass);
    }
    if (descriptor->mixins != NULL){
        for (i = 0; descriptor->mixins[i] != NULL; i++){
            LT_Class* mixin = descriptor->mixins[i];

            LT_init_native_class(mixin);
            if ((mixin->class_flags & LT_CLASS_FLAG_ABSTRACT) == 0){
                LT_error("Native class mixin must be abstract");
            }
            if (mixin->instance_size != 0){
                LT_error("Native class mixin must be zero-sized");
            }
            if ((mixin->class_flags & LT_CLASS_FLAG_FLEXIBLE) != 0){
                LT_error("Native class mixin must not be flexible");
            }
        }
    }
    if (descriptor->metaclass_superclass != NULL){
        LT_init_native_class(descriptor->metaclass_superclass);
    }

    metaclass = klass->base.klass;

    klass->instance_size = descriptor->instance_size;
    klass->class_flags = (unsigned int)descriptor->class_flags;
    klass->debugPrintOn = descriptor->debugPrintOn;
    if (descriptor->hash != NULL){
        klass->hash = descriptor->hash;
    } else if (descriptor->superclass != NULL){
        klass->hash = descriptor->superclass->hash;
    } else {
        klass->hash = Class_default_hash;
    }
    if (descriptor->equal_p != NULL){
        klass->equal_p = descriptor->equal_p;
    } else if (descriptor->superclass != NULL){
        klass->equal_p = descriptor->superclass->equal_p;
    } else {
        klass->equal_p = Class_default_equal_p;
    }
    klass->name = native_class_name_symbol(descriptor);
    LT_init_native_class(&LT_IdentityDictionary_class);
    klass->methods = (LT_Value)(uintptr_t)LT_IdentityDictionary_new();
    klass->method_cache = (LT_Value)(uintptr_t)LT_IdentityDictionary_new();
    klass->documentation =
        materialize_documentation(klass, descriptor->documentation);
    klass->superclasses = make_native_superclass_list(descriptor);
    invalidate_inline_caches();
    LT_ilc_epoch_copy_acquire_release(&klass->method_cache_epoch);
    klass->precedence_list = make_precedence_list(klass);
    materialize_slots(klass, descriptor->superclass, descriptor->slots);
    materialize_direct_methods(klass, descriptor->methods);

    if (metaclass != NULL){
        metaclass->base.klass = &LT_Class_class_class;
        if (metaclass->instance_size == 0){
            metaclass->instance_size = sizeof(LT_Class);
        }
        if (metaclass->debugPrintOn == NULL){
            metaclass->debugPrintOn = Class_debugPrintOn;
        }
        if (descriptor->metaclass_superclass != NULL){
            metaclass->hash = descriptor->metaclass_superclass->hash;
            metaclass->equal_p = descriptor->metaclass_superclass->equal_p;
        } else {
            metaclass->hash = Class_default_hash;
            metaclass->equal_p = Class_default_equal_p;
        }
        metaclass->name = make_metaclass_name(klass->name);
        LT_init_native_class(&LT_IdentityDictionary_class);
        metaclass->methods = (LT_Value)(uintptr_t)LT_IdentityDictionary_new();
        metaclass->method_cache = (LT_Value)(uintptr_t)LT_IdentityDictionary_new();
        LT_ilc_epoch_copy_acquire_release(&metaclass->method_cache_epoch);
        metaclass->documentation = LT_NIL;
        metaclass->superclasses =
            make_single_superclass_list(descriptor->metaclass_superclass);
        metaclass->precedence_list = make_single_inheritance_precedence_list(
            metaclass,
            descriptor->metaclass_superclass
        );
        materialize_direct_methods(
            metaclass,
            descriptor->class_methods
        );
        if (descriptor->metaclass_superclass != NULL){
            metaclass->slot_count = descriptor->metaclass_superclass->slot_count;
            metaclass->slots = descriptor->metaclass_superclass->slots;
        } else {
            metaclass->slot_count = 0;
            metaclass->slots = NULL;
        }
    }

    if (!core_class_cycle_finalized){
        record_early_native_class(klass);
    }
    finalize_core_class_cycle_if_ready();
}

LT_Value LT_Class_new(LT_Value name, LT_Value superclasses, LT_Value slot_names){
    LT_Class* klass;
    LT_Class* metaclass;
    LT_Class** superclass_array;
    LT_Class** metaclass_superclass_array;
    size_t superclass_count;
    LT_Class* primary_superclass = NULL;
    LT_Class* primary_metaclass_superclass;

    if (name != LT_NIL && !LT_Symbol_p(name)){
        LT_type_error(name, &LT_Symbol_class);
    }

    superclass_array = materialize_superclass_array(superclasses, &superclass_count);
    if (superclass_count != 0){
        primary_superclass = superclass_array[0];
    }
    metaclass_superclass_array = materialize_metaclass_superclass_array(
        superclass_array,
        superclass_count
    );
    primary_metaclass_superclass = metaclass_superclass_array[0];

    LT_init_native_class(&LT_IdentityDictionary_class);
    metaclass = LT_Class_ALLOC(LT_Class);
    metaclass->base.klass = &LT_Class_class_class;
    metaclass->superclasses = metaclass_superclass_array;
    metaclass->precedence_list = make_precedence_list(metaclass);
    metaclass->instance_size = sizeof(LT_Class);
    metaclass->class_flags = 0;
    if (primary_metaclass_superclass != NULL){
        metaclass->slot_count = primary_metaclass_superclass->slot_count;
        metaclass->slots = primary_metaclass_superclass->slots;
        metaclass->hash = primary_metaclass_superclass->hash;
        metaclass->equal_p = primary_metaclass_superclass->equal_p;
    } else {
        metaclass->slot_count = 0;
        metaclass->slots = NULL;
        metaclass->hash = Class_default_hash;
        metaclass->equal_p = Class_default_equal_p;
    }
    metaclass->methods = (LT_Value)(uintptr_t)LT_IdentityDictionary_new();
    metaclass->method_cache = (LT_Value)(uintptr_t)LT_IdentityDictionary_new();
    LT_ilc_epoch_copy_acquire_release(&metaclass->method_cache_epoch);
    metaclass->name = make_metaclass_name(name);
    metaclass->debugPrintOn = Class_debugPrintOn;
    metaclass->documentation = LT_NIL;
    metaclass->native_descriptor = NULL;

    klass = LT_Class_ALLOC(LT_Class);
    klass->base.klass = metaclass;
    klass->superclasses = superclass_array;
    invalidate_inline_caches();
    klass->precedence_list = make_precedence_list(klass);
    klass->class_flags = LT_CLASS_FLAG_ALLOCATABLE;
    klass->methods = (LT_Value)(uintptr_t)LT_IdentityDictionary_new();
    klass->method_cache = (LT_Value)(uintptr_t)LT_IdentityDictionary_new();
    LT_ilc_epoch_copy_acquire_release(&klass->method_cache_epoch);
    klass->name = name;
    klass->debugPrintOn = Class_debugPrintOn;
    if (primary_superclass != NULL){
        klass->hash = primary_superclass->hash;
        klass->equal_p = primary_superclass->equal_p;
    } else {
        klass->hash = Class_default_hash;
        klass->equal_p = Class_default_equal_p;
    }
    klass->documentation = LT_NIL;
    klass->native_descriptor = NULL;
    materialize_dynamic_slots(klass, superclass_array, slot_names);

    return (LT_Value)(uintptr_t)klass;
}

LT_Value LT_Class_make_instance(LT_Class* klass){
    if ((klass->class_flags & LT_CLASS_FLAG_ALLOCATABLE) == 0){
        LT_error("Class is not allocatable");
    }

    return (LT_Value)(uintptr_t)LT_Class_alloc(klass);
}

LT_Class_Slot* LT_Class_lookup_slot(LT_Class* klass, LT_Value slot_name){
    size_t low = 0;
    size_t high = klass->slot_count;

    if (!LT_Symbol_p(slot_name)){
        LT_type_error(slot_name, &LT_Symbol_class);
    }

    while (low < high){
        size_t mid = low + ((high - low) / 2);
        LT_Value candidate = klass->slots[mid].name;

        if (candidate == slot_name){
            return &klass->slots[mid];
        }
        if (candidate < slot_name){
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    return NULL;
}

LT_Value LT_Class_slots(LT_Class* klass){
    LT_ListBuilder* builder = LT_ListBuilder_new();
    size_t i;

    for (i = 0; i < klass->slot_count; i++){
        LT_ListBuilder_append(builder, klass->slots[i].name);
    }

    return LT_ListBuilder_value(builder);
}

static void append_selector_to_builder(LT_Value selector, void* baton){
    LT_ListBuilder* builder = (LT_ListBuilder*)baton;

    LT_ListBuilder_append(builder, selector);
}

static void put_selector_in_set(LT_Value selector, void* baton){
    LT_Set* set = (LT_Set*)baton;

    LT_Set_put(set, selector);
}

static LT_Value class_direct_selectors_as_list(LT_Class* klass){
    LT_ListBuilder* builder = LT_ListBuilder_new();

    LT_IdentityDictionary_keys_do(
        LT_IdentityDictionary_from_value(klass->methods),
        append_selector_to_builder,
        builder
    );
    return LT_ListBuilder_value(builder);
}

static LT_Value class_direct_method(LT_Class* klass, LT_Value selector){
    LT_IdentityDictionary* methods;
    LT_Value method;

    if (!LT_Symbol_p(selector)){
        LT_type_error(selector, &LT_Symbol_class);
    }

    methods = LT_IdentityDictionary_from_value(klass->methods);
    if (!LT_IdentityDictionary_at(methods, selector, &method)){
        return LT_NIL;
    }
    return method;
}

struct LT_Class_MethodReflectionBaton {
    LT_Class* klass;
    LT_Value callable;
    LT_ListBuilder* builder;
    LT_IdentitySet* seen;
};

static LT_Value class_all_methods_as_list(LT_Class* klass);

static LT_Value class_method_descriptor(LT_Class* klass, LT_Value selector){
    LT_Value callable;

    if (!LT_IdentityDictionary_at(
        LT_IdentityDictionary_from_value(klass->methods),
        selector,
        &callable
    )){
        return LT_NIL;
    }

    return LT_MethodDescriptor_new(
        selector,
        callable,
        (LT_Value)(uintptr_t)klass
    );
}

static void class_direct_method_do(LT_Value selector, void* baton_value){
    struct LT_Class_MethodReflectionBaton* baton =
        (struct LT_Class_MethodReflectionBaton*)baton_value;
    LT_Value method = class_method_descriptor(baton->klass, selector);

    (void)LT_apply(baton->callable, LT_cons(method, LT_NIL), LT_NIL, LT_NIL, NULL);
}

static void class_direct_method_append(LT_Value selector, void* baton_value){
    struct LT_Class_MethodReflectionBaton* baton =
        (struct LT_Class_MethodReflectionBaton*)baton_value;

    LT_ListBuilder_append(
        baton->builder,
        class_method_descriptor(baton->klass, selector)
    );
}

static void class_all_method_do(LT_Value selector, void* baton_value){
    struct LT_Class_MethodReflectionBaton* baton =
        (struct LT_Class_MethodReflectionBaton*)baton_value;
    LT_Value method;

    if (LT_Set_contains((LT_Set*)baton->seen, selector)){
        return;
    }
    LT_Set_put((LT_Set*)baton->seen, selector);
    method = class_method_descriptor(baton->klass, selector);
    (void)LT_apply(baton->callable, LT_cons(method, LT_NIL), LT_NIL, LT_NIL, NULL);
}

static void class_all_method_append(LT_Value selector, void* baton_value){
    struct LT_Class_MethodReflectionBaton* baton =
        (struct LT_Class_MethodReflectionBaton*)baton_value;

    if (LT_Set_contains((LT_Set*)baton->seen, selector)){
        return;
    }
    LT_Set_put((LT_Set*)baton->seen, selector);
    LT_ListBuilder_append(
        baton->builder,
        class_method_descriptor(baton->klass, selector)
    );
}

static void class_direct_methods_do(LT_Class* klass, LT_Value callable){
    struct LT_Class_MethodReflectionBaton baton = {
        .klass = klass,
        .callable = callable,
        .builder = NULL,
        .seen = NULL,
    };

    LT_IdentityDictionary_keys_do(
        LT_IdentityDictionary_from_value(klass->methods),
        class_direct_method_do,
        &baton
    );
}

static LT_Value class_direct_methods_as_list(LT_Class* klass){
    LT_ListBuilder* builder = LT_ListBuilder_new();
    struct LT_Class_MethodReflectionBaton baton = {
        .klass = klass,
        .callable = LT_NIL,
        .builder = builder,
        .seen = NULL,
    };

    LT_IdentityDictionary_keys_do(
        LT_IdentityDictionary_from_value(klass->methods),
        class_direct_method_append,
        &baton
    );
    return LT_ListBuilder_value(builder);
}

LT_PRIMITIVE_HEAD(class_method_inspection){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value methods;
    LT_Value base_inspection_value;
    LT_ObjectInspection* base_inspection;
    LT_ListBuilder* contents = LT_ListBuilder_new();
    LT_Class* klass;
    LT_Value name;
    LT_Value description;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    klass = LT_Class_from_object(self);
    methods = class_all_methods_as_list(klass);
    while (methods != LT_NIL){
        LT_Value method = LT_car(methods);
        LT_ListBuilder_append(
            contents,
            LT_MethodDescriptor_selector(LT_MethodDescriptor_from_value(method))
        );
        LT_ListBuilder_append(contents, method);
        methods = LT_cdr(methods);
    }
    base_inspection_value = LT_Object_inspection(self);
    base_inspection = LT_ObjectInspection_from_value(base_inspection_value);
    name = (LT_Value)(uintptr_t)LT_String_new_cstr(
        LT_sprintf(
            "Class %s",
            LT_Symbol_name(LT_Symbol_from_value(klass->name))
        )
    );
    description = LT_ObjectInspection_description(base_inspection);
    return LT_ObjectInspection_new(
        name,
        description,
        LT_ObjectInspection_slots(base_inspection),
        (LT_Value)(uintptr_t)LT_String_new_cstr("Methods:"),
        LT_ListBuilder_value(contents)
    );
}

static void class_all_methods_do(LT_Class* klass, LT_Value callable){
    LT_Value precedence_cursor = LT_Class_precedence_list(klass);
    LT_IdentitySet* seen = LT_IdentitySet_new();

    while (precedence_cursor != LT_NIL){
        LT_Value class_value = LT_ImmutableList_car(precedence_cursor);
        LT_Class* current = LT_Class_from_object(class_value);
        struct LT_Class_MethodReflectionBaton baton = {
            .klass = current,
            .callable = callable,
            .builder = NULL,
            .seen = seen,
        };

        LT_IdentityDictionary_keys_do(
            LT_IdentityDictionary_from_value(current->methods),
            class_all_method_do,
            &baton
        );
        precedence_cursor = LT_ImmutableList_cdr(precedence_cursor);
    }
}

static LT_Value class_all_methods_as_list(LT_Class* klass){
    LT_Value precedence_cursor = LT_Class_precedence_list(klass);
    LT_IdentitySet* seen = LT_IdentitySet_new();
    LT_ListBuilder* builder = LT_ListBuilder_new();

    while (precedence_cursor != LT_NIL){
        LT_Value class_value = LT_ImmutableList_car(precedence_cursor);
        LT_Class* current = LT_Class_from_object(class_value);
        struct LT_Class_MethodReflectionBaton baton = {
            .klass = current,
            .callable = LT_NIL,
            .builder = builder,
            .seen = seen,
        };

        LT_IdentityDictionary_keys_do(
            LT_IdentityDictionary_from_value(current->methods),
            class_all_method_append,
            &baton
        );
        precedence_cursor = LT_ImmutableList_cdr(precedence_cursor);
    }

    return LT_ListBuilder_value(builder);
}

static LT_IdentitySet* class_direct_selectors(LT_Class* klass){
    LT_IdentitySet* selectors = LT_IdentitySet_new();

    LT_IdentityDictionary_keys_do(
        LT_IdentityDictionary_from_value(klass->methods),
        put_selector_in_set,
        (LT_Set*)selectors
    );
    return selectors;
}

static LT_IdentitySet* class_all_selectors(LT_Class* klass){
    LT_IdentitySet* selectors = LT_IdentitySet_new();
    LT_Value precedence_cursor = LT_Class_precedence_list(klass);

    while (precedence_cursor != LT_NIL){
        LT_Value class_value = LT_ImmutableList_car(precedence_cursor);
        LT_Class* current = LT_Class_from_object(class_value);

        LT_IdentityDictionary_keys_do(
            LT_IdentityDictionary_from_value(current->methods),
            put_selector_in_set,
            (LT_Set*)selectors
        );
        precedence_cursor = LT_ImmutableList_cdr(precedence_cursor);
    }

    return selectors;
}

void LT_Class_addMethod(LT_Class* klass, LT_Value selector, LT_Value method){
    LT_IdentityDictionary* methods;

    if (!LT_Symbol_p(selector)){
        LT_type_error(selector, &LT_Symbol_class);
    }
    if (!LT_Primitive_p(method) && !LT_Closure_p(method)){
        LT_error("Method must be primitive or closure");
    }

    methods = LT_IdentityDictionary_from_value(klass->methods);
    LT_IdentityDictionary_atPut(methods, selector, method);

    invalidate_inline_caches();
}

LT_Value LT_Class_lookup_method(LT_Class* klass, LT_Value selector){
    return LT_Class_lookup_method_with_next(klass, selector, NULL);
}

LT_Value LT_Class_lookup_method_with_next(LT_Class* klass,
                                          LT_Value selector,
                                          LT_Value* next_precedence_out){
    LT_IdentityDictionary* method_cache;
    LT_Value precedence_cursor;
    LT_Value method;
    int method_cached;

    if (!LT_Symbol_p(selector)){
        LT_type_error(selector, &LT_Symbol_class);
    }

    if (!LT_ilc_epoch_equals_acquire(&klass->method_cache_epoch)){
        klass->method_cache = (LT_Value)(uintptr_t)LT_IdentityDictionary_new();
        LT_ilc_epoch_copy_acquire_release(&klass->method_cache_epoch);
    }

    method_cache = LT_IdentityDictionary_from_value(klass->method_cache);
    method_cached = LT_IdentityDictionary_at(method_cache, selector, &method);
    if (method_cached){
        precedence_cursor = LT_Class_precedence_list(klass);
    } else {
        precedence_cursor = LT_Class_precedence_list(klass);
    }

    while (precedence_cursor != LT_NIL){
        LT_Value class_value = LT_ImmutableList_car(precedence_cursor);
        LT_Class* current = LT_Class_from_object(class_value);
        LT_IdentityDictionary* methods = LT_IdentityDictionary_from_value(
            current->methods
        );

        if (LT_IdentityDictionary_at(methods, selector, &method)){
            if (!method_cached){
                LT_IdentityDictionary_atPut(method_cache, selector, method);
            }
            if (next_precedence_out != NULL){
                *next_precedence_out = LT_ImmutableList_cdr(precedence_cursor);
            }
            return method;
        }

        precedence_cursor = LT_ImmutableList_cdr(precedence_cursor);
    }

    if (next_precedence_out != NULL){
        *next_precedence_out = LT_NIL;
    }
    return LT_INVALID;
}

LT_PRIMITIVE_HEAD(class_method_slots){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Class_slots(LT_Class_from_object(self));
}

LT_PRIMITIVE_HEAD(class_method_documentation){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Class_from_object(self)->documentation;
}

LT_PRIMITIVE_HEAD(class_method_precedence_list){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return LT_Class_precedence_list(LT_Class_from_object(self));
}

LT_PRIMITIVE_HEAD(class_method_binary_lookup_selector){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value selector;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, selector);
    LT_ARG_END(cursor);

    return class_direct_method(LT_Class_from_object(self), selector);
}

LT_PRIMITIVE_HEAD(class_method_lookup_selector){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value selector;
    LT_Value method;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, selector);
    LT_ARG_END(cursor);

    method = LT_Class_lookup_method(LT_Class_from_object(self), selector);
    if (method == LT_INVALID){
        return LT_NIL;
    }
    return method;
}

LT_PRIMITIVE_HEAD(class_method_selectors){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)class_direct_selectors(LT_Class_from_object(self));
}

LT_PRIMITIVE_HEAD(class_method_all_selectors){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)class_all_selectors(LT_Class_from_object(self));
}

LT_PRIMITIVE_HEAD(class_method_selectors_as_list){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return class_direct_selectors_as_list(LT_Class_from_object(self));
}

LT_PRIMITIVE_HEAD(class_method_all_selectors_as_list){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_IdentitySet* selectors;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    selectors = class_all_selectors(LT_Class_from_object(self));
    return LT_Set_asList((LT_Set*)selectors);
}

LT_PRIMITIVE_HEAD(class_method_methods_do){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);

    class_direct_methods_do(LT_Class_from_object(self), callable);
    return LT_NIL;
}

LT_PRIMITIVE_HEAD(class_method_methods_as_list){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return class_direct_methods_as_list(LT_Class_from_object(self));
}

LT_PRIMITIVE_HEAD(class_method_all_methods_do){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value callable;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, callable);
    LT_ARG_END(cursor);

    class_all_methods_do(LT_Class_from_object(self), callable);
    return LT_NIL;
}

LT_PRIMITIVE_HEAD(class_method_all_methods_as_list){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    return class_all_methods_as_list(LT_Class_from_object(self));
}

LT_PRIMITIVE_HEAD(class_method_add_method_with_selector){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value method;
    LT_Value selector;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, method);
    LT_OBJECT_ARG(cursor, selector);
    LT_ARG_END(cursor);

    LT_Class_addMethod(LT_Class_from_object(self), selector, method);
    return method;
}

LT_PRIMITIVE_HEAD(class_method_alloc){
    LT_Value cursor = arguments;
    LT_Value self;
    (void)tail_call_unwind_marker;

    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);

    return LT_Class_make_instance(LT_Class_from_object(self));
}
