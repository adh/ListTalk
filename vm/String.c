#include <ListTalk/String.h>
#include <string.h>

struct LT_String_s {
    LT_ObjectHeader base;
    size_t length;
    char str[];
};

LT_DEFINE_CLASS(LT_String){
    .instance_size = sizeof(LT_String),
};

LT_String* LT_String_new(char* buf, size_t len){
    LT_String* str = LT_Class_ALLOC_FLEXIBLE(LT_String, len + 1);
    str->length = len;
    memcpy(str->str, buf, len);
    str->str[len] = '\0';
    return str;
}
LT_String* LT_String_new_cstr(char* str){
    return LT_String_new(str, strlen(str));
}
char* LT_String_value_cstr(LT_String* string){
    return string->str;
}
