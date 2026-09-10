/* SPDX-License-Identifier: MIT */
#include "Socket_internal.h"

#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/eval.h>

#include <errno.h>
#include <string.h>
#include <sys/socket.h>

#define SOCKET_READ_BUFFER_SIZE 4096

void LT_socket_read_buffer_init(LT_SocketReadBuffer* buffer){
    buffer->bytes = NULL;
    buffer->start = 0;
    buffer->end = 0;
    buffer->capacity = 0;
}

static size_t buffered_length(LT_SocketReadBuffer* buffer){
    return buffer->end - buffer->start;
}

static ssize_t socket_receive(int fd,
                              void* destination,
                              size_t length,
                              const char* error_message){
    ssize_t count;

    while (1){
        count = recv(fd, destination, length, 0);
        if (count >= 0 || errno != EINTR){
            break;
        }
        LT_socket_interrupted();
    }
    if (count < 0){
        LT_system_error(error_message, errno);
    }
    return count;
}

void LT_socket_interrupted(void){
    LT_check_pending_signal();
}

static int fill_buffer(int fd,
                       LT_SocketReadBuffer* buffer,
                       const char* error_message){
    ssize_t count;

    if (buffer->capacity == 0){
        buffer->bytes = GC_MALLOC_ATOMIC(SOCKET_READ_BUFFER_SIZE);
        buffer->capacity = SOCKET_READ_BUFFER_SIZE;
    }
    count = socket_receive(
        fd,
        buffer->bytes,
        buffer->capacity,
        error_message
    );
    buffer->start = 0;
    buffer->end = (size_t)count;
    return count != 0;
}

size_t LT_socket_buffered_read(int fd,
                               LT_SocketReadBuffer* buffer,
                               void* destination,
                               size_t length,
                               const char* error_message){
    uint8_t* output = destination;
    size_t total = 0;

    while (total < length){
        size_t available = buffered_length(buffer);

        if (available > 0){
            size_t count = available < length - total
                ? available
                : length - total;

            memcpy(output + total, buffer->bytes + buffer->start, count);
            buffer->start += count;
            total += count;
        } else {
            ssize_t count = socket_receive(
                fd,
                output + total,
                length - total,
                error_message
            );

            if (count == 0){
                break;
            }
            total += (size_t)count;
        }
    }
    return total;
}

static void append_bytes(uint8_t** result,
                         size_t* length,
                         size_t* capacity,
                         const uint8_t* bytes,
                         size_t count){
    if (count > *capacity - *length){
        size_t new_capacity = *capacity ? *capacity : SOCKET_READ_BUFFER_SIZE;
        uint8_t* new_result;

        while (count > new_capacity - *length){
            new_capacity *= 2;
        }
        new_result = GC_MALLOC_ATOMIC(new_capacity);
        if (*length > 0){
            memcpy(new_result, *result, *length);
        }
        *result = new_result;
        *capacity = new_capacity;
    }
    memcpy(*result + *length, bytes, count);
    *length += count;
}

LT_Value LT_socket_buffered_read_line(int fd,
                                      LT_SocketReadBuffer* buffer,
                                      const char* error_message){
    uint8_t* result = NULL;
    size_t length = 0;
    size_t capacity = 0;

    while (1){
        size_t available;
        uint8_t* delimiter;
        size_t count;

        if (buffered_length(buffer) == 0
                && !fill_buffer(fd, buffer, error_message)){
            if (length == 0){
                return LT_NIL;
            }
            return (LT_Value)(uintptr_t)LT_ByteVector_new(result, length);
        }
        available = buffered_length(buffer);
        delimiter = memchr(buffer->bytes + buffer->start, '\n', available);
        count = delimiter == NULL
            ? available
            : (size_t)(delimiter - (buffer->bytes + buffer->start)) + 1;
        append_bytes(
            &result,
            &length,
            &capacity,
            buffer->bytes + buffer->start,
            count
        );
        buffer->start += count;
        if (delimiter != NULL){
            return (LT_Value)(uintptr_t)LT_ByteVector_new(result, length);
        }
    }
}
