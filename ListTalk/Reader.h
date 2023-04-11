#ifndef H__ListTalk__reader__
#define H__ListTalk__reader__

#include <ListTalk/OOP.h>

LT_DECLARE_CLASS(LT_Reader);

extern LT_Reader* LT_Reader_newForStream(FILE* stream);
extern LT_Object* LT_Reader_readObject(LT_Reader* reader);

#endif