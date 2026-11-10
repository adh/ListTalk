/* SPDX-License-Identifier: MIT */
#include "Socket_internal.h"

#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/eval.h>

#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

#define SOCKET_READ_BUFFER_SIZE 4096

void LT_socket_read_buffer_init(LT_SocketReadBuffer* buffer){
    buffer->bytes = NULL;
    buffer->start = 0;
    buffer->end = 0;
    buffer->capacity = 0;
    buffer->has_read_deadline = 0;
}

void LT_socket_read_buffer_set_timeout_ms(LT_SocketReadBuffer* buffer,
                                          unsigned timeout_ms){
    if (!timeout_ms){
        buffer->has_read_deadline = 0;
        return;
    }
    if (clock_gettime(CLOCK_MONOTONIC, &buffer->read_deadline) != 0){
        LT_system_error("Could not read monotonic clock", errno);
    }
    buffer->read_deadline.tv_sec += timeout_ms / 1000;
    buffer->read_deadline.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (buffer->read_deadline.tv_nsec >= 1000000000L){
        buffer->read_deadline.tv_sec++;
        buffer->read_deadline.tv_nsec -= 1000000000L;
    }
    buffer->has_read_deadline = 1;
}

static size_t buffered_length(LT_SocketReadBuffer* buffer){
    return buffer->end - buffer->start;
}

static void wait_for_readable(int fd,
                              LT_SocketReadBuffer* buffer,
                              const char* error_message){
    struct pollfd descriptor = {.fd = fd, .events = POLLIN};

    while (buffer->has_read_deadline){
        struct timespec now;
        int64_t remaining_ns;
        int timeout;
        int result;

        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0){
            LT_system_error("Could not read monotonic clock", errno);
        }
        remaining_ns =
            ((int64_t)buffer->read_deadline.tv_sec - (int64_t)now.tv_sec)
                * 1000000000
            + buffer->read_deadline.tv_nsec - now.tv_nsec;
        if (remaining_ns <= 0){
            LT_system_error(error_message, ETIMEDOUT);
        }
        remaining_ns = (remaining_ns + 999999) / 1000000;
        timeout = remaining_ns > INT_MAX ? INT_MAX : (int)remaining_ns;
        result = poll(&descriptor, 1, timeout);
        if (result > 0){
            return;
        }
        if (result == 0){
            LT_system_error(error_message, ETIMEDOUT);
        }
        if (errno != EINTR){
            LT_system_error(error_message, errno);
        }
        LT_socket_interrupted();
    }
}

static ssize_t socket_receive(int fd,
                              LT_SocketReadBuffer* buffer,
                              void* destination,
                              size_t length,
                              const char* error_message){
    ssize_t count;

    while (1){
        wait_for_readable(fd, buffer, error_message);
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
        buffer,
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
                buffer,
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

LT_Value LT_socket_buffered_read_line_limited(int fd,
                                              LT_SocketReadBuffer* buffer,
                                              size_t maximum_length,
                                              int* limit_exceeded,
                                              const char* error_message){
    uint8_t* result = NULL;
    size_t length = 0;
    size_t capacity = 0;

    *limit_exceeded = 0;
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
        if (count > maximum_length - length){
            *limit_exceeded = 1;
            return LT_NIL;
        }
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

LT_Value LT_socket_buffered_read_line(int fd,
                                      LT_SocketReadBuffer* buffer,
                                      const char* error_message){
    int limit_exceeded;

    return LT_socket_buffered_read_line_limited(
        fd,
        buffer,
        SIZE_MAX,
        &limit_exceeded,
        error_message
    );
}
