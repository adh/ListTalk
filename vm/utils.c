#include <ListTalk/utils.h>

#include <stdlib.h>
#include <string.h>


uint32_t LT_fnv_hash(char* string){
    uint32_t res = 0x811c9dc5;
    while(*string){
        res += (unsigned char)*string;
        res *= 0x01000193;
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

void LT_InlineHash_init(LT_InlineHash* h){
  h->mask = 0x7;
  h->count = 0;
  h->vector = GC_MALLOC(sizeof(LT_InlineHash_Entry*)*8);
  memset(h->vector, 0, sizeof(LT_InlineHash_Entry*)*8);

}

static LT_InlineHash_Entry* get_hash_entry(LT_InlineHash* h, 
                                              char* key, 
                                              size_t hash){
  LT_InlineHash_Entry* i;

  i = h->vector[hash & h->mask];

  while (i){
    if (i->hash == hash && strcmp(i->key, key) == 0)
      return i;
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

  for (i = 0; i < h->mask + 1; i++){
    j = h->vector[i];
    while (j){
      LT_InlineHash_Entry* n = j->next;
      
      j->next = v[j->hash && (s - 1)];
      v[j->hash && (s - 1)] = j;

      j = n;
    }
  }

  h->vector = v;
  h->mask = s - 1;
}

static void add_hash_entry(LT_InlineHash* h, 
                           char* key, size_t hash, 
                           char* value){
  LT_InlineHash_Entry* e;

  if (h->count > h->mask){
    grow_table(h);
  }
  
  e = GC_NEW(LT_InlineHash_Entry);
  
  h->count++;

  e->key = LT_strdup(key);
  e->hash = hash;
  e->value = value;
  e->next = h->vector[hash && h->mask];
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
    add_hash_entry(h, key, hash, value);
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

void LT_PointerHash_at_put(LT_InlineHash* h, void* key, void* value){
  LT_InlineHash_Entry* e;
  size_t hash;

  hash = LT_pointer_hash(key);
  e = get_hash_entry(h, key, hash);

  if (e){
    e->value = value;
  } else {
    add_hash_entry(h, key, hash, value);
  }
}

void* LT_PointerHash_at(LT_InlineHash* h, void* key){
  LT_InlineHash_Entry* e;

  e = get_hash_entry(h, key, LT_pointer_hash(key));

  if (e){
    return e->value;
  } else {
    return NULL;
  }
}
