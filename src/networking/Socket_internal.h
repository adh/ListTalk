/* SPDX-License-Identifier: MIT */
#ifndef H__ListTalk__networking__Socket_internal__
#define H__ListTalk__networking__Socket_internal__

#include <ListTalk/vm/value.h>

typedef struct LT_SocketReadBuffer {
    uint8_t* bytes;
    size_t start;
    size_t end;
    size_t capacity;
} LT_SocketReadBuffer;

void LT_socket_read_buffer_init(LT_SocketReadBuffer* buffer);
size_t LT_socket_buffered_read(int fd,
                               LT_SocketReadBuffer* buffer,
                               void* destination,
                               size_t length,
                               const char* error_message);
LT_Value LT_socket_buffered_read_line(int fd,
                                      LT_SocketReadBuffer* buffer,
                                      const char* error_message);
void LT_socket_interrupted(void);

#endif
