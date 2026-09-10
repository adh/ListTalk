/* SPDX-License-Identifier: MIT */
#include <ListTalk/ListTalk.h>
#include <ListTalk/networking/Socket.h>
#include <ListTalk/classes/Class.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/error.h>

#include "Socket_internal.h"

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

struct LT_UnixSocket_s {
    LT_Object base;
    int fd;
};

struct LT_UnixDatagramSocket_s {
    LT_UnixSocket base;
};

struct LT_UnixStreamSocket_s {
    LT_UnixSocket base;
    LT_SocketReadBuffer read_buffer;
};

struct LT_UnixServerSocket_s {
    LT_UnixSocket base;
};

static int unix_socket_fd(LT_UnixSocket* socket){
    if (socket->fd < 0){
        LT_error("Socket is closed");
    }
    return socket->fd;
}

static LT_UnixSocket* unix_socket_from_value(LT_Value value){
    if (LT_Value_class(value) != &LT_UnixSocket_class
            && !LT_Value_is_instance_of(
                value,
                LT_STATIC_CLASS(LT_UnixSocket)
            )){
        LT_type_error(value, &LT_UnixSocket_class);
    }
    return (LT_UnixSocket*)LT_VALUE_POINTER_VALUE(value);
}

int LT_UnixSocket_closed(LT_UnixSocket* socket){
    return socket->fd < 0;
}

int LT_UnixSocket_descriptor(LT_UnixSocket* socket){
    return unix_socket_fd(socket);
}

void LT_UnixSocket_close(LT_UnixSocket* socket){
    int fd;

    if (socket->fd < 0){
        return;
    }
    fd = socket->fd;
    socket->fd = -1;
    if (close(fd) != 0 && errno != EINTR){
        LT_system_error("Socket close failed", errno);
    }
}

static void unix_socket_finalizer(void* object, void* unused){
    LT_UnixSocket* socket = object;

    (void)unused;
    if (socket->fd >= 0){
        close(socket->fd);
        socket->fd = -1;
    }
}

static void unix_socket_set_fd(LT_UnixSocket* socket, int fd){
    socket->fd = fd;
    GC_register_finalizer(socket, unix_socket_finalizer, NULL, NULL, NULL);
}

static socklen_t unix_address(const char* path, struct sockaddr_un* address){
    size_t length = strlen(path);

    if (length == 0){
        LT_error("Unix socket path must not be empty");
    }
    if (length >= sizeof(address->sun_path)){
        LT_error("Unix socket path is too long");
    }
    memset(address, 0, sizeof(*address));
    address->sun_family = AF_UNIX;
    memcpy(address->sun_path, path, length + 1);
    return (socklen_t)(offsetof(struct sockaddr_un, sun_path) + length + 1);
}

static int unix_connect(const char* path, int type){
    struct sockaddr_un address;
    socklen_t address_length = unix_address(path, &address);
    int fd = socket(AF_UNIX, type, 0);

    if (fd < 0){
        LT_system_error("Could not create Unix socket", errno);
    }
    if (connect(fd, (struct sockaddr*)&address, address_length) != 0){
        int saved_errno = errno;

        close(fd);
        LT_system_error("Could not connect Unix socket", saved_errno);
    }
    return fd;
}

static int unix_bind(const char* path, int type, int backlog){
    struct sockaddr_un address;
    socklen_t address_length = unix_address(path, &address);
    int fd = socket(AF_UNIX, type, 0);

    if (fd < 0){
        LT_system_error("Could not create Unix socket", errno);
    }
    if (bind(fd, (struct sockaddr*)&address, address_length) != 0
            || (backlog > 0 && listen(fd, backlog) != 0)){
        int saved_errno = errno;

        close(fd);
        LT_system_error("Could not bind Unix socket", saved_errno);
    }
    return fd;
}

LT_UnixDatagramSocket* LT_UnixDatagramSocket_bind(const char* path){
    LT_UnixDatagramSocket* result = LT_Class_ALLOC(LT_UnixDatagramSocket);

    unix_socket_set_fd((LT_UnixSocket*)result, unix_bind(path, SOCK_DGRAM, 0));
    return result;
}

LT_UnixDatagramSocket* LT_UnixDatagramSocket_connect(const char* path){
    LT_UnixDatagramSocket* result = LT_Class_ALLOC(LT_UnixDatagramSocket);

    unix_socket_set_fd(
        (LT_UnixSocket*)result,
        unix_connect(path, SOCK_DGRAM)
    );
    return result;
}

size_t LT_UnixDatagramSocket_send(LT_UnixDatagramSocket* socket,
                                  LT_ByteVector* bytes){
    ssize_t count;

    while (1){
        count = send(
            unix_socket_fd((LT_UnixSocket*)socket),
            LT_ByteVector_bytes(bytes),
            LT_ByteVector_length(bytes),
            0
        );
        if (count >= 0 || errno != EINTR){
            break;
        }
        LT_socket_interrupted();
    }
    if (count < 0){
        LT_system_error("Unix datagram send failed", errno);
    }
    if ((size_t)count != LT_ByteVector_length(bytes)){
        LT_error("Unix datagram send was incomplete");
    }
    return (size_t)count;
}

LT_ByteVector* LT_UnixDatagramSocket_receive(
    LT_UnixDatagramSocket* socket,
    size_t maximum_length
){
    uint8_t* bytes = GC_MALLOC_ATOMIC(
        maximum_length ? maximum_length : 1
    );
    ssize_t count;

    while (1){
        count = recv(
            unix_socket_fd((LT_UnixSocket*)socket),
            bytes,
            maximum_length,
            0
        );
        if (count >= 0 || errno != EINTR){
            break;
        }
        LT_socket_interrupted();
    }
    if (count < 0){
        LT_system_error("Unix datagram receive failed", errno);
    }
    return LT_ByteVector_new(bytes, (size_t)count);
}

static LT_UnixStreamSocket* unix_stream_from_fd(int fd){
    LT_UnixStreamSocket* result = LT_Class_ALLOC(LT_UnixStreamSocket);

    unix_socket_set_fd((LT_UnixSocket*)result, fd);
    LT_socket_read_buffer_init(&result->read_buffer);
    return result;
}

LT_UnixStreamSocket* LT_UnixStreamSocket_connect(const char* path){
    return unix_stream_from_fd(unix_connect(path, SOCK_STREAM));
}

void LT_UnixStreamSocket_pair(LT_UnixStreamSocket** first,
                              LT_UnixStreamSocket** second){
    int descriptors[2];

    while (socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors) != 0){
        if (errno != EINTR){
            LT_system_error("Could not create Unix socket pair", errno);
        }
        LT_socket_interrupted();
    }
    *first = unix_stream_from_fd(descriptors[0]);
    *second = unix_stream_from_fd(descriptors[1]);
}

size_t LT_UnixStreamSocket_read(LT_UnixStreamSocket* socket,
                                void* buffer,
                                size_t length){
    return LT_socket_buffered_read(
        unix_socket_fd((LT_UnixSocket*)socket),
        &socket->read_buffer,
        buffer,
        length,
        "Unix socket read failed"
    );
}

LT_Value LT_UnixStreamSocket_readLine(LT_UnixStreamSocket* socket){
    return LT_socket_buffered_read_line(
        unix_socket_fd((LT_UnixSocket*)socket),
        &socket->read_buffer,
        "Unix socket read failed"
    );
}

void LT_UnixStreamSocket_write(LT_UnixStreamSocket* socket,
                               const void* buffer,
                               size_t length){
    const uint8_t* bytes = buffer;
    size_t offset = 0;

    while (offset < length){
#ifdef MSG_NOSIGNAL
        ssize_t count = send(
            unix_socket_fd((LT_UnixSocket*)socket),
            bytes + offset,
            length - offset,
            MSG_NOSIGNAL
        );
#else
        ssize_t count = send(
            unix_socket_fd((LT_UnixSocket*)socket),
            bytes + offset,
            length - offset,
            0
        );
#endif
        if (count < 0 && errno == EINTR){
            LT_socket_interrupted();
            continue;
        }
        if (count <= 0){
            LT_system_error(
                "Unix socket write failed",
                count < 0 ? errno : EPIPE
            );
        }
        offset += (size_t)count;
    }
}

void LT_UnixStreamSocket_shutdown_write(LT_UnixStreamSocket* socket){
    if (shutdown(unix_socket_fd((LT_UnixSocket*)socket), SHUT_WR) != 0){
        LT_system_error("Unix socket shutdown failed", errno);
    }
}

LT_UnixServerSocket* LT_UnixServerSocket_new(const char* path, int backlog){
    LT_UnixServerSocket* result;

    if (backlog < 1){
        LT_error("Socket backlog must be positive");
    }
    result = LT_Class_ALLOC(LT_UnixServerSocket);
    unix_socket_set_fd(
        (LT_UnixSocket*)result,
        unix_bind(path, SOCK_STREAM, backlog)
    );
    return result;
}

LT_UnixStreamSocket* LT_UnixServerSocket_accept(
    LT_UnixServerSocket* socket
){
    int fd;

    while (1){
        fd = accept(unix_socket_fd((LT_UnixSocket*)socket), NULL, NULL);
        if (fd >= 0 || errno != EINTR){
            break;
        }
        LT_socket_interrupted();
    }
    if (fd < 0){
        LT_system_error("Unix socket accept failed", errno);
    }
    return unix_stream_from_fd(fd);
}

static LT_String* unix_path_arg(LT_Value* cursor){
    LT_String* path;

    LT_GENERIC_ARG(*cursor, path, LT_String*, LT_String_from_value);
    return path;
}

LT_DEFINE_PRIMITIVE(
    unix_socket_closed,
    "UnixSocket>>isClosed",
    "(self)",
    "Return true when the Unix socket is closed."
){
    LT_Value cursor = arguments;
    LT_UnixSocket* self;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, self, LT_UnixSocket*, unix_socket_from_value);
    LT_ARG_END(cursor);
    return LT_UnixSocket_closed(self) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    unix_socket_close,
    "UnixSocket>>close",
    "(self)",
    "Close the Unix socket."
){
    LT_Value cursor = arguments;
    LT_UnixSocket* self;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, self, LT_UnixSocket*, unix_socket_from_value);
    LT_ARG_END(cursor);
    LT_UnixSocket_close(self);
    return (LT_Value)(uintptr_t)self;
}

#define UNIX_PATH_CONSTRUCTOR(c_name, primitive_name, function)            \
LT_DEFINE_PRIMITIVE(                                                       \
    c_name, primitive_name, "(self path)", "Create a Unix socket."       \
){                                                                         \
    LT_Value cursor = arguments;                                           \
    LT_Value self;                                                         \
    LT_String* path;                                                       \
                                                                            \
    (void)tail_call_unwind_marker;                                         \
    LT_OBJECT_ARG(cursor, self);                                           \
    path = unix_path_arg(&cursor);                                         \
    LT_ARG_END(cursor);                                                    \
    (void)self;                                                            \
    return (LT_Value)(uintptr_t)function(LT_String_value_cstr(path));       \
}

UNIX_PATH_CONSTRUCTOR(
    unix_datagram_bind,
    "UnixDatagramSocket class>>bindTo:",
    LT_UnixDatagramSocket_bind
)
UNIX_PATH_CONSTRUCTOR(
    unix_datagram_connect,
    "UnixDatagramSocket class>>connectTo:",
    LT_UnixDatagramSocket_connect
)
UNIX_PATH_CONSTRUCTOR(
    unix_stream_connect,
    "UnixStreamSocket class>>connectTo:",
    LT_UnixStreamSocket_connect
)

#undef UNIX_PATH_CONSTRUCTOR

LT_DEFINE_PRIMITIVE(
    unix_datagram_send,
    "UnixDatagramSocket>>send:",
    "(self bytes)",
    "Send one Unix datagram."
){
    LT_Value cursor = arguments;
    LT_UnixDatagramSocket* self;
    LT_ByteVector* bytes;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(
        cursor,
        self,
        LT_UnixDatagramSocket*,
        LT_UnixDatagramSocket_from_value
    );
    LT_GENERIC_ARG(cursor, bytes, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    return LT_Number_smallinteger_from_size(
        LT_UnixDatagramSocket_send(self, bytes),
        "Datagram too large"
    );
}

LT_DEFINE_PRIMITIVE(
    unix_datagram_receive,
    "UnixDatagramSocket>>receive:",
    "(self maximumLength)",
    "Receive one Unix datagram."
){
    LT_Value cursor = arguments;
    LT_Value maximum_length;
    LT_UnixDatagramSocket* self;
    size_t length;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(
        cursor,
        self,
        LT_UnixDatagramSocket*,
        LT_UnixDatagramSocket_from_value
    );
    LT_OBJECT_ARG(cursor, maximum_length);
    LT_ARG_END(cursor);
    length = LT_Number_nonnegative_size_from_integer(
        maximum_length,
        "Invalid receive length",
        "Invalid receive length"
    );
    return (LT_Value)(uintptr_t)LT_UnixDatagramSocket_receive(self, length);
}

LT_DEFINE_PRIMITIVE(
    unix_stream_pair,
    "UnixStreamSocket class>>pair",
    "(self)",
    "Create and return a connected pair of Unix stream sockets."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_UnixStreamSocket* first;
    LT_UnixStreamSocket* second;

    (void)tail_call_unwind_marker;
    LT_OBJECT_ARG(cursor, self);
    LT_ARG_END(cursor);
    (void)self;
    LT_UnixStreamSocket_pair(&first, &second);
    return LT_cons(
        (LT_Value)(uintptr_t)first,
        LT_cons((LT_Value)(uintptr_t)second, LT_NIL)
    );
}

LT_DEFINE_PRIMITIVE(
    unix_stream_read,
    "UnixStreamSocket>>read:",
    "(self maximumLength)",
    "Read bytes from a Unix stream socket."
){
    LT_Value cursor = arguments;
    LT_Value maximum_length;
    LT_UnixStreamSocket* self;
    size_t length;
    size_t count;
    uint8_t* bytes;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(
        cursor,
        self,
        LT_UnixStreamSocket*,
        LT_UnixStreamSocket_from_value
    );
    LT_OBJECT_ARG(cursor, maximum_length);
    LT_ARG_END(cursor);
    length = LT_Number_nonnegative_size_from_integer(
        maximum_length,
        "Invalid read length",
        "Invalid read length"
    );
    bytes = GC_MALLOC_ATOMIC(length ? length : 1);
    count = LT_UnixStreamSocket_read(self, bytes, length);
    return (LT_Value)(uintptr_t)LT_ByteVector_new(bytes, count);
}

LT_DEFINE_PRIMITIVE(
    unix_stream_write,
    "UnixStreamSocket>>write:",
    "(self bytes)",
    "Write bytes to a Unix stream socket."
){
    LT_Value cursor = arguments;
    LT_UnixStreamSocket* self;
    LT_ByteVector* bytes;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(
        cursor,
        self,
        LT_UnixStreamSocket*,
        LT_UnixStreamSocket_from_value
    );
    LT_GENERIC_ARG(cursor, bytes, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    LT_UnixStreamSocket_write(
        self,
        LT_ByteVector_bytes(bytes),
        LT_ByteVector_length(bytes)
    );
    return (LT_Value)(uintptr_t)self;
}

LT_DEFINE_PRIMITIVE(
    unix_stream_read_line,
    "UnixStreamSocket>>readLine",
    "(self)",
    "Read through a line feed and include it in the returned bytevector."
){
    LT_Value cursor = arguments;
    LT_UnixStreamSocket* self;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(
        cursor,
        self,
        LT_UnixStreamSocket*,
        LT_UnixStreamSocket_from_value
    );
    LT_ARG_END(cursor);
    return LT_UnixStreamSocket_readLine(self);
}

LT_DEFINE_PRIMITIVE(
    unix_stream_shutdown,
    "UnixStreamSocket>>shutdownWrite",
    "(self)",
    "Shut down the writing half of a Unix stream socket."
){
    LT_Value cursor = arguments;
    LT_UnixStreamSocket* self;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(
        cursor,
        self,
        LT_UnixStreamSocket*,
        LT_UnixStreamSocket_from_value
    );
    LT_ARG_END(cursor);
    LT_UnixStreamSocket_shutdown_write(self);
    return (LT_Value)(uintptr_t)self;
}

LT_DEFINE_PRIMITIVE(
    unix_server_new,
    "UnixServerSocket class>>newOn:backlog:",
    "(self path backlog)",
    "Create a listening Unix socket."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value backlog_value;
    LT_String* path;
    size_t backlog;

    (void)tail_call_unwind_marker;
    LT_OBJECT_ARG(cursor, self);
    path = unix_path_arg(&cursor);
    LT_OBJECT_ARG(cursor, backlog_value);
    LT_ARG_END(cursor);
    backlog = LT_Number_nonnegative_size_from_integer(
        backlog_value,
        "Invalid socket backlog",
        "Invalid socket backlog"
    );
    if (backlog < 1 || backlog > INT_MAX){
        LT_error("Invalid socket backlog");
    }
    (void)self;
    return (LT_Value)(uintptr_t)LT_UnixServerSocket_new(
        LT_String_value_cstr(path),
        (int)backlog
    );
}

LT_DEFINE_PRIMITIVE(
    unix_server_new_default_backlog,
    "UnixServerSocket class>>newOn:",
    "(self path)",
    "Create a listening Unix socket using the system maximum backlog."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* path;

    (void)tail_call_unwind_marker;
    LT_OBJECT_ARG(cursor, self);
    path = unix_path_arg(&cursor);
    LT_ARG_END(cursor);
    (void)self;
    return (LT_Value)(uintptr_t)LT_UnixServerSocket_new(
        LT_String_value_cstr(path),
        SOMAXCONN
    );
}

LT_DEFINE_PRIMITIVE(
    unix_server_accept,
    "UnixServerSocket>>accept",
    "(self)",
    "Accept and return a Unix stream socket."
){
    LT_Value cursor = arguments;
    LT_UnixServerSocket* self;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(
        cursor,
        self,
        LT_UnixServerSocket*,
        LT_UnixServerSocket_from_value
    );
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_UnixServerSocket_accept(self);
}

static LT_Method_Descriptor unix_socket_methods[] = {
    {"isClosed", &unix_socket_closed},
    {"close", &unix_socket_close},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor unix_datagram_methods[] = {
    {"send:", &unix_datagram_send},
    {"receive:", &unix_datagram_receive},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor unix_datagram_class_methods[] = {
    {"bindTo:", &unix_datagram_bind},
    {"connectTo:", &unix_datagram_connect},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor unix_stream_methods[] = {
    {"read:", &unix_stream_read},
    {"readLine", &unix_stream_read_line},
    {"write:", &unix_stream_write},
    {"shutdownWrite", &unix_stream_shutdown},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor unix_stream_class_methods[] = {
    {"connectTo:", &unix_stream_connect},
    {"pair", &unix_stream_pair},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor unix_server_methods[] = {
    {"accept", &unix_server_accept},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor unix_server_class_methods[] = {
    {"newOn:", &unix_server_new_default_backlog},
    {"newOn:backlog:", &unix_server_new},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Class* unix_datagram_mixins[] = {
    &LT_DatagramSocket_class,
    NULL
};

static LT_Class* unix_stream_mixins[] = {
    &LT_StreamSocket_class,
    NULL
};

static LT_Class* unix_server_mixins[] = {
    &LT_ServerSocket_class,
    NULL
};

LT_DEFINE_CLASS(LT_UnixSocket) {
    .superclass = &LT_Socket_class,
    .metaclass_superclass = &LT_Class_class,
    .package = "ListTalk:Socket",
    .name = "UnixSocket",
    .documentation = "Abstract Unix-domain socket.",
    .instance_size = sizeof(LT_UnixSocket),
    .class_flags = LT_CLASS_FLAG_ABSTRACT,
    .methods = unix_socket_methods,
};

LT_DEFINE_CLASS(LT_UnixDatagramSocket) {
    .superclass = &LT_UnixSocket_class,
    .mixins = unix_datagram_mixins,
    .metaclass_superclass = &LT_Class_class,
    .package = "ListTalk:Socket",
    .name = "UnixDatagramSocket",
    .documentation = "Unix-domain datagram socket.",
    .instance_size = sizeof(LT_UnixDatagramSocket),
    .methods = unix_datagram_methods,
    .class_methods = unix_datagram_class_methods,
};

LT_DEFINE_CLASS(LT_UnixStreamSocket) {
    .superclass = &LT_UnixSocket_class,
    .mixins = unix_stream_mixins,
    .metaclass_superclass = &LT_Class_class,
    .package = "ListTalk:Socket",
    .name = "UnixStreamSocket",
    .documentation = "Connected Unix-domain stream socket.",
    .instance_size = sizeof(LT_UnixStreamSocket),
    .methods = unix_stream_methods,
    .class_methods = unix_stream_class_methods,
};

LT_DEFINE_CLASS(LT_UnixServerSocket) {
    .superclass = &LT_UnixSocket_class,
    .mixins = unix_server_mixins,
    .metaclass_superclass = &LT_Class_class,
    .package = "ListTalk:Socket",
    .name = "UnixServerSocket",
    .documentation = "Listening Unix-domain stream socket.",
    .instance_size = sizeof(LT_UnixServerSocket),
    .methods = unix_server_methods,
    .class_methods = unix_server_class_methods,
};
