#include <ListTalk/OOP.h>


void Nil_printOn(LT_Object* obj, FILE* stream){
    fputs("#n", stream);
}

LT_DEFINE_CLASS(LT_Nil){
    .printOn = Nil_printOn,
};

extern LT_Class* LT_Object_class(LT_Object *object);
extern void LT_Object_printOn(LT_Object* obj, FILE* stream);

LT_Object* LT_Class_alloc(LT_Class *klass)
{
    LT_Object* obj = GC_MALLOC(klass->instance_size);
    obj->klass = klass;
    return obj;
}
LT_Object* LT_Class_alloc_flexible(LT_Class *klass, size_t flex)
{
    LT_Object* obj = GC_MALLOC(klass->instance_size + flex);
    obj->klass = klass;
    return obj;
}
