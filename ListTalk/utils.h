#ifndef H__ListTalk__utils__
#define H__ListTalk__utils__

#include <ListTalk/env_macros.h>

#include <stdint.h>

LT__BEGIN_DECLS

    extern uint32_t LT_fnv_hash(char* string);
    extern uint32_t LT_pointer_hash(void* pointer);

    extern char* LT_strdup(char*);

    typedef struct LT_StringBuilder LT_StringBuilder;

    LT_StringBuilder* LT_StringBuilder_new();
    void LT_StringBuilder_append_str(LT_StringBuilder* builder, char*str);
    void LT_StringBuilder_append_char(LT_StringBuilder* builder, char ch);
    char* LT_StringBuilder_value(LT_StringBuilder* builder);
    size_t LT_StringBuilder_length(LT_StringBuilder* builder);


    typedef struct LT_InlineHash LT_InlineHash;
    typedef struct LT_InlineHash_Entry LT_InlineHash_Entry;


    struct LT_InlineHash {
        LT_InlineHash_Entry** vector;
        size_t mask;
        size_t count;
    };

    struct LT_InlineHash_Entry {
        size_t hash;
        void* key;
        void* value;
        LT_InlineHash_Entry* next;
    };

    void LT_InlineHash_init(LT_InlineHash* h);


    void LT_StringHash_at_put(LT_InlineHash* sh,
                              char* key, void* value);
    void* LT_StringHash_at(LT_InlineHash* sh, char* key);

    void LT_PointerHash_at_put(LT_InlineHash* sh,
                               void* key, void* value);
    void* LT_PointerHash_at(LT_InlineHash* sh,
                            void* key);


LT__END_DECLS

#endif