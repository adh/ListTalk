#include <ListTalk/Object.h>


void Nil_printOn(LT_Object* obj, FILE* stream){
    fputs("#n", stream);
}

LT_DEFINE_CLASS(LT_Nil){
    .printOn = Nil_printOn,
};

void* LT_Class_alloc(LT_Class *klass)
{
    LT_ObjectHeader* obj = GC_MALLOC(klass->instance_size);
    obj->klass = klass;
    return obj;
}
void* LT_Class_alloc_flexible(LT_Class *klass, size_t flex)
{
    LT_ObjectHeader* obj = GC_MALLOC(klass->instance_size + flex);
    obj->klass = klass;
    return obj;
}
