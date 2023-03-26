#include <ListTalk/OOP.h>

extern LT_Class* LT_OOP_class(LT_Object *object);


LT_Object* LT_Class_alloc(LT_Class *klass)
{
    LT_Object* obj = GC_MALLOC(klass->instance_size);
    obj->klass = klass;
    return obj;
}
