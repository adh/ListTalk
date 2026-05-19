/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/utils.h>
#include <ListTalk/classes/Pair.h>
#include <ListTalk/vm/error.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


uint32_t LT_fnv_hash(char* string){
    uint32_t res = 0x811c9dc5;
    while(*string){
        res += (unsigned char)*string;
        res *= 0x01000193;
        string++;
    }
    return res;
}

uint32_t LT_pointer_hash(void* pointer){
    uint32_t res = 0x811c9dc5;
    size_t buf = (size_t)pointer;
    buf >>= 3;
    while(buf){
        res += buf & 0xfff;
        buf >>= 12;
        res *= 0x01000193;
    }
    return res;
}

extern char* LT_strdup(char* string){
    size_t len = strlen(string);
    char* res = GC_MALLOC_ATOMIC(len+1);
    memcpy(res, string, len);
    res[len] = 0;
    return res;
}

extern char* LT_sprintf(const char* fmt, ...){
    va_list args;
    int size;
    char* buf;

    va_start(args, fmt);
    size = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (size < 0){
        return NULL;
    }

    buf = GC_MALLOC_ATOMIC((size_t)size + 1);

    va_start(args, fmt);
    vsnprintf(buf, (size_t)size + 1, fmt, args);
    va_end(args);

    return buf;
}

void LT_write_file_bytes_atomically(const char* path,
                                    const void* bytes,
                                    size_t length){
    const unsigned char* byte_cursor = bytes;
    size_t path_length = strlen(path);
    const char* suffix = ".tmp.XXXXXX";
    size_t suffix_length = strlen(suffix);
    char* temp_path = GC_MALLOC_ATOMIC(path_length + suffix_length + 1);
    int fd;
    size_t offset = 0;

    memcpy(temp_path, path, path_length);
    memcpy(temp_path + path_length, suffix, suffix_length + 1);

    fd = mkstemp(temp_path);
    if (fd < 0){
        LT_system_error("Could not create temporary file", errno);
    }

    while (offset < length){
        size_t chunk = length - offset;
        ssize_t written;

        if (chunk > (size_t)SSIZE_MAX){
            chunk = (size_t)SSIZE_MAX;
        }
        written = write(fd, byte_cursor + offset, chunk);
        if (written < 0){
            int saved_errno = errno;

            close(fd);
            unlink(temp_path);
            LT_system_error("Could not write file", saved_errno);
        }
        if (written == 0){
            close(fd);
            unlink(temp_path);
            LT_error("Could not write file");
        }
        offset += (size_t)written;
    }

    if (close(fd) != 0){
        int saved_errno = errno;

        unlink(temp_path);
        LT_system_error("Could not close file", saved_errno);
    }
    if (rename(temp_path, path) != 0){
        int saved_errno = errno;

        unlink(temp_path);
        LT_system_error("Could not replace file", saved_errno);
    }
}

#define STRING_BUILDER_INIT_SIZE 64

struct LT_StringBuilder {
  size_t length;
  size_t size;
  char* buf;
};

struct LT_ListBuilder {
    LT_Value head;
    LT_Value tail;
    size_t length;
};

LT_StringBuilder* LT_StringBuilder_new(){
    LT_StringBuilder* builder = GC_NEW(LT_StringBuilder);
    builder->size = STRING_BUILDER_INIT_SIZE;
    builder->buf = GC_MALLOC_ATOMIC(STRING_BUILDER_INIT_SIZE);
    builder->length = 0;
    builder->buf[builder->length] = 0;
    return builder;
}


void LT_StringBuilder_append_str(LT_StringBuilder* builder, char*str){
    /* TODO: do this in at least minimaly sane way */
    while(*str){
        LT_StringBuilder_append_char(builder, *str);
        str++;
    }
}

void LT_StringBuilder_append_bytes(LT_StringBuilder* builder,
                                   const char* bytes,
                                   size_t length){
    size_t required_size;

    if (length == 0){
        return;
    }

    required_size = builder->length + length + 1;
    if (builder->size < required_size){
        size_t new_size = builder->size * 2 + 2;
        char* new_buf;

        while (new_size < required_size){
            new_size = new_size * 2 + 2;
        }

        new_buf = GC_MALLOC_ATOMIC(new_size);
        memcpy(new_buf, builder->buf, builder->length + 1);
        builder->size = new_size;
        builder->buf = new_buf;
    }

    memcpy(builder->buf + builder->length, bytes, length);
    builder->length += length;
    builder->buf[builder->length] = '\0';
}

void LT_StringBuilder_append_char(LT_StringBuilder* builder, char ch){
    if (builder->size < builder->length + 2){
        size_t new_size = builder->size * 2 + 2;
        char* new_buf = GC_MALLOC_ATOMIC(new_size);
        memcpy(new_buf, builder->buf, builder->size);
        builder->size = new_size;
        builder->buf = new_buf;
    }
    builder->buf[builder->length] = ch;
    builder->length++;
    builder->buf[builder->length] = '\0';
}
char* LT_StringBuilder_value(LT_StringBuilder* builder){
    return builder->buf;
}
size_t LT_StringBuilder_length(LT_StringBuilder* builder){
    return builder->length;
}

LT_ListBuilder* LT_ListBuilder_new(){
    LT_ListBuilder* builder = GC_NEW(LT_ListBuilder);
    builder->head = LT_NIL;
    builder->tail = LT_NIL;
    builder->length = 0;
    return builder;
}

void LT_ListBuilder_append(LT_ListBuilder* builder, LT_Value value){
    LT_Value pair = LT_cons(value, LT_NIL);

    if (builder->length == 0){
        builder->head = pair;
    } else {
        LT_Pair_set_cdr(builder->tail, pair);
    }

    builder->tail = pair;
    builder->length++;
}

LT_Value LT_ListBuilder_value(LT_ListBuilder* builder){
    return builder->head;
}

LT_Value LT_ListBuilder_valueWithRest(LT_ListBuilder* builder, LT_Value rest){
    if (builder->length == 0){
        return rest;
    }
    LT_Pair_set_cdr(builder->tail, rest);
    return builder->head;
}

size_t LT_ListBuilder_length(LT_ListBuilder* builder){
    return builder->length;
}



void LT_InlineHash_init(LT_InlineHash* h){
    h->mask = 0x7;
    h->count = 0;
    h->vector = GC_MALLOC(sizeof(LT_InlineHash_Entry*)*8);
    memset(h->vector, 0, sizeof(LT_InlineHash_Entry*)*8);

}

size_t LT_InlineHash_count(LT_InlineHash* h){
    return h->count;
}

static LT_InlineHash_Entry* get_hash_entry(LT_InlineHash* h, 
                                              char* key, 
                                              size_t hash){
    LT_InlineHash_Entry* i;

    i = h->vector[hash & h->mask];

    while (i){
        if (i->hash == hash && strcmp(i->key, key) == 0){
            return i;
        }
        i = i->next;
    }
    return NULL;
}

static void grow_table(LT_InlineHash* h){
    LT_InlineHash_Entry** v;
    size_t s;
    size_t i;
    LT_InlineHash_Entry* j;

    s = (h->mask + 1) << 1;
    v = GC_MALLOC(sizeof(LT_InlineHash_Entry*) * s);
    memset(v, 0, sizeof(LT_InlineHash_Entry*) * s);

    for (i = 0; i < h->mask + 1; i++){
        j = h->vector[i];
        while (j){
            LT_InlineHash_Entry* n = j->next;
      
            j->next = v[j->hash & (s - 1)];
            v[j->hash & (s - 1)] = j;

            j = n;
        }
    }

    h->vector = v;
    h->mask = s - 1;
}

static void add_hash_entry_string(LT_InlineHash* h, 
                                  char* key, size_t hash, 
                                  void* value){
    LT_InlineHash_Entry* e;

    if (h->count > h->mask){
        grow_table(h);
    }

    e = GC_NEW(LT_InlineHash_Entry);

    h->count++;

    e->key = LT_strdup(key);
    e->hash = hash;
    e->value = value;
    e->next = h->vector[hash & h->mask];
    h->vector[hash & h->mask] = e;
}

static LT_InlineHash_Entry* get_hash_entry_pointer(LT_InlineHash* h,
                                                   void* key,
                                                   size_t hash){
    LT_InlineHash_Entry* i;

    i = h->vector[hash & h->mask];

    while (i){
        if (i->hash == hash && i->key == key){
            return i;
        }
        i = i->next;
    }
    return NULL;
}

static void add_hash_entry_pointer(LT_InlineHash* h,
                                   void* key,
                                   size_t hash,
                                   void* value){
    LT_InlineHash_Entry* e;

    if (h->count > h->mask){
        grow_table(h);
    }

    e = GC_NEW(LT_InlineHash_Entry);

    h->count++;

    e->key = key;
    e->hash = hash;
    e->value = value;
    e->next = h->vector[hash & h->mask];
    h->vector[hash & h->mask] = e;
}

void LT_StringHash_at_put(LT_InlineHash* h, char* key, void* value){
    LT_InlineHash_Entry* e;
    size_t hash;

    hash = LT_fnv_hash(key);
    e = get_hash_entry(h, key, hash);

    if (e){
        e->value = value;
    } else {
        add_hash_entry_string(h, key, hash, value);
    }
}

void* LT_StringHash_at(LT_InlineHash* h, char* key){
    LT_InlineHash_Entry* e;

    e = get_hash_entry(h, key, LT_fnv_hash(key));

    if (e){
        return e->value;
    } else {
        return NULL;
    }
}

int LT_StringHash_remove(LT_InlineHash* h, char* key, void** value_out){
    size_t hash = LT_fnv_hash(key);
    size_t index = hash & h->mask;
    LT_InlineHash_Entry* current = h->vector[index];
    LT_InlineHash_Entry* previous = NULL;

    while (current != NULL){
        if (current->hash == hash && strcmp(current->key, key) == 0){
            if (previous == NULL){
                h->vector[index] = current->next;
            } else {
                previous->next = current->next;
            }

            h->count--;
            if (value_out != NULL){
                *value_out = current->value;
            }
            return 1;
        }
        previous = current;
        current = current->next;
    }

    return 0;
}

void LT_PointerHash_at_put(LT_InlineHash* h, void* key, void* value){
    LT_InlineHash_Entry* e;
    size_t hash;

    hash = LT_pointer_hash(key);
    e = get_hash_entry_pointer(h, key, hash);

    if (e){
        e->value = value;
    } else {
        add_hash_entry_pointer(h, key, hash, value);
    }
}

void* LT_PointerHash_at(LT_InlineHash* h, void* key){
    LT_InlineHash_Entry* e;

    e = get_hash_entry_pointer(h, key, LT_pointer_hash(key));

    if (e){
        return e->value;
    } else {
        return NULL;
    }
}

int LT_PointerHash_remove(LT_InlineHash* h, void* key, void** value_out){
    size_t hash = LT_pointer_hash(key);
    size_t index = hash & h->mask;
    LT_InlineHash_Entry* current = h->vector[index];
    LT_InlineHash_Entry* previous = NULL;

    while (current != NULL){
        if (current->hash == hash && current->key == key){
            if (previous == NULL){
                h->vector[index] = current->next;
            } else {
                previous->next = current->next;
            }

            h->count--;
            if (value_out != NULL){
                *value_out = current->value;
            }
            return 1;
        }
        previous = current;
        current = current->next;
    }

    return 0;
}

void LT_register_constructor(void (*ctor)(void)){
    /* TODO: handle VM initialization internal states */
    ctor();
}
