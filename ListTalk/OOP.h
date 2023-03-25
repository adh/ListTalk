#ifndef H__ListTalk__OOP__
#define H__ListTalk__OOP__

typedef struct LT_Object LT_Object;

struct LT_Object{
    LT_Object* type;
};

#define LT_OOP_NIL NULL
#define LT_OOP_INVALID (LT_Object*)(~((size_t)NULL))

#endif