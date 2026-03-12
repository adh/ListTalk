/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/vm/Class.h>
#include <ListTalk/classes/Closure.h>
#include <ListTalk/classes/IdentityDictionary.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/utils.h>

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

static uintptr_t LT_Class_method_cache_global_version = 1;

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
    class_method_lookup_selector,
    "Class>>lookupSelector:",
    "(self selector)",
    "Return resolved method for selector or nil."
);

static LT_Method_Descriptor Class_methods[] = {
    {"slots", &class_method_slots},
    {"lookupSelector:", &class_method_lookup_selector},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Class) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Class",
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

static LT_Value* make_single_inheritance_precedence_list(LT_Class* klass,
                                                         LT_Class* superclass){
    size_t superclass_length = 0;
    LT_Value* precedence_list;
    size_t i;

    if (superclass != NULL && superclass->precedence_list != NULL){
        while (superclass->precedence_list[superclass_length] != LT_INVALID){
            superclass_length++;
        }
    }

    precedence_list = GC_MALLOC(
        sizeof(LT_Value) * (superclass_length + 2)
    );
    precedence_list[0] = (LT_Value)(uintptr_t)klass;
    for (i = 0; i < superclass_length; i++){
        precedence_list[i + 1] = superclass->precedence_list[i];
    }
    precedence_list[superclass_length + 1] = LT_INVALID;
    return precedence_list;
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

static LT_Value make_metaclass_name(LT_Value class_name){
    char* name;
    size_t length;
    char* metaclass_name;

    if (!LT_Symbol_p(class_name)){
        return LT_NIL;
    }

    name = LT_Symbol_name(LT_Symbol_from_value(class_name));
    length = strlen(name);
    metaclass_name = GC_MALLOC_ATOMIC(length + strlen(" class") + 1);
    memcpy(metaclass_name, name, length);
    memcpy(metaclass_name + length, " class", strlen(" class") + 1);
    return LT_Symbol_new(metaclass_name);
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

static void materialize_slots(LT_Class* klass, LT_Slot_Descriptor* descriptor_slots){
    size_t superclass_slot_count = 0;
    size_t descriptor_slot_count;
    size_t max_slot_count;
    size_t slot_count = 0;
    LT_Class_Slot* slots;
    size_t i;

    if (klass->superclasses != NULL && klass->superclasses[0] != NULL){
        superclass_slot_count = klass->superclasses[0]->slot_count;
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
            klass->superclasses[0]->slots,
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

void LT_init_native_class(LT_Class* klass){
    LT_Class_Descriptor* descriptor = klass->native_descriptor;
    LT_Class* metaclass;

    if (descriptor == NULL){
        return;
    }

    /* Mark as in-progress before recursive initialization to break cycles. */
    klass->native_descriptor = NULL;

    if (descriptor->superclass != NULL){
        LT_init_native_class(descriptor->superclass);
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
    if (descriptor->name == NULL){
        klass->name = LT_NIL;
    } else {
        klass->name = LT_Symbol_new(descriptor->name);
    }
    LT_init_native_class(&LT_IdentityDictionary_class);
    klass->methods = (LT_Value)(uintptr_t)LT_IdentityDictionary_new();
    klass->method_cache = (LT_Value)(uintptr_t)LT_IdentityDictionary_new();
    klass->cache_version = LT_Class_method_cache_global_version;
    klass->documentation = LT_NIL;
    klass->superclasses = make_single_superclass_list(descriptor->superclass);
    klass->precedence_list = make_single_inheritance_precedence_list(
        klass,
        descriptor->superclass
    );
    materialize_slots(klass, descriptor->slots);
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
        metaclass->cache_version = LT_Class_method_cache_global_version;
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

    LT_Class_method_cache_global_version++;
}

LT_Value LT_Class_lookup_method(LT_Class* klass, LT_Value selector){
    LT_IdentityDictionary* method_cache;
    LT_Value method;
    size_t i;

    if (!LT_Symbol_p(selector)){
        LT_type_error(selector, &LT_Symbol_class);
    }

    if (klass->cache_version != LT_Class_method_cache_global_version){
        klass->method_cache = (LT_Value)(uintptr_t)LT_IdentityDictionary_new();
        klass->cache_version = LT_Class_method_cache_global_version;
    }

    method_cache = LT_IdentityDictionary_from_value(klass->method_cache);
    if (LT_IdentityDictionary_at(method_cache, selector, &method)){
        return method;
    }

    for (i = 0; klass->precedence_list[i] != LT_INVALID; i++){
        LT_Class* current = LT_Class_from_object(klass->precedence_list[i]);
        LT_IdentityDictionary* methods = LT_IdentityDictionary_from_value(
            current->methods
        );

        if (LT_IdentityDictionary_at(methods, selector, &method)){
            LT_IdentityDictionary_atPut(method_cache, selector, method);
            return method;
        }
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
