#ifndef H__ListTalk__reader__
#define H__ListTalk__reader__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Reader);

typedef struct LT_ReaderStream_s LT_ReaderStream;
typedef struct LT_ReaderStreamVTable_s {
    int (*getc)(void* stream);
    int (*ungetc)(int c, void* stream);
} LT_ReaderStreamVTable;

struct LT_ReaderStream_s {
    LT_ReaderStreamVTable* vtable;
};

static inline int LT_ReaderStream_getc(LT_ReaderStream* stream){
    return stream->vtable->getc(stream);
}
static inline int LT_ReaderStream_ungetc(LT_ReaderStream* stream, int c){
    return stream->vtable->ungetc(c, stream);
}

extern LT_ReaderStream* LT_ReaderStream_newForFile(FILE* file);
extern LT_ReaderStream* LT_ReaderStream_newForString(const char* str);

extern LT_Reader* LT_Reader_new();
extern LT_Reader* LT_Reader_clone(LT_Reader* reader);
extern LT_Value LT_Reader_readObject(
    LT_Reader* reader,
    LT_ReaderStream* stream
);



LT__END_DECLS

#endif
