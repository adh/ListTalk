/* SPDX-License-Identifier: MIT */
#include <ListTalk/ListTalk.h>
#include <ListTalk/networking/Socket.h>
#include <ListTalk/classes/Class.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/classes/SmallInteger.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/error.h>

#include "Socket_internal.h"

#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

struct LT_IPSocket_s {
    LT_Object base;
    int fd;
};

struct LT_UDPSocket_s {
    LT_IPSocket base;
};

struct LT_TCPSocket_s {
    LT_IPSocket base;
    LT_SocketReadBuffer read_buffer;
};

struct LT_TCPServerSocket_s {
    LT_IPSocket base;
};

static int socket_fd(LT_IPSocket* socket){
    if (socket->fd < 0){
        LT_error("Socket is closed");
    }
    return socket->fd;
}

static LT_IPSocket* socket_from_value(LT_Value value){
    if (LT_Value_class(value) != &LT_IPSocket_class
            && !LT_Value_is_instance_of(value, LT_STATIC_CLASS(LT_IPSocket)))
        LT_type_error(value, &LT_IPSocket_class);
    return (LT_IPSocket*)LT_VALUE_POINTER_VALUE(value);
}

int LT_IPSocket_closed(LT_IPSocket* socket){
    return socket->fd < 0;
}

int LT_IPSocket_descriptor(LT_IPSocket* socket){
    return socket_fd(socket);
}

void LT_IPSocket_close(LT_IPSocket* socket){
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

static void socket_finalizer(void* object, void* unused){
    LT_IPSocket* socket = object;
    (void)unused;
    if (socket->fd >= 0){
        close(socket->fd);
        socket->fd = -1;
    }
}

static void set_fd(LT_IPSocket* socket, int fd){
    socket->fd = fd;
    GC_register_finalizer(socket, socket_finalizer, NULL, NULL, NULL);
}

static uint16_t port_value(LT_Value value){
    size_t port = LT_Number_nonnegative_size_from_integer(
        value,
        "Invalid socket port",
        "Invalid socket port"
    );

    if (port > 65535){
        LT_error("Socket port must be an integer from 0 through 65535");
    }
    return (uint16_t)port;
}

static int resolve_socket(const char* host,
                          uint16_t port,
                          int type,
                          int passive,
                          int do_listen,
                          int backlog){
    struct addrinfo hints = {0};
    struct addrinfo* addresses;
    struct addrinfo* address;
    char service[6];
    int fd = -1;
    int result;
    int saved_errno = 0;
    int one = 1;
    int zero = 0;
    const char* resolved_host = host;

    if (passive && host != NULL && strcmp(host, "*") == 0){
        resolved_host = NULL;
    }

    snprintf(service, sizeof(service), "%u", (unsigned)port);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = type;
    hints.ai_flags = passive ? AI_PASSIVE : 0;
    result = getaddrinfo(
        resolved_host && *resolved_host ? resolved_host : NULL,
        service,
        &hints,
        &addresses
    );
    if (result != 0){
        LT_error(LT_sprintf(
            "Could not resolve socket address: %s",
            gai_strerror(result)
        ));
    }
    for (address = addresses; address; address = address->ai_next){
        fd = socket(
            address->ai_family,
            address->ai_socktype,
            address->ai_protocol
        );
        if (fd < 0){
            saved_errno = errno;
            continue;
        }
        if (do_listen
                && setsockopt(
                    fd,
                    SOL_SOCKET,
                    SO_REUSEADDR,
                    &one,
                    sizeof(one)
                ) != 0){
            saved_errno = errno;
            close(fd);
            fd = -1;
            continue;
        }
        if (do_listen
                && address->ai_family == AF_INET6
                && setsockopt(
                    fd,
                    IPPROTO_IPV6,
                    IPV6_V6ONLY,
                    &zero,
                    sizeof(zero)
                ) != 0){
            saved_errno = errno;
            close(fd);
            fd = -1;
            continue;
        }
        int ready = 0;

        while (1){
            result = passive
                ? bind(fd, address->ai_addr, address->ai_addrlen)
                : connect(fd, address->ai_addr, address->ai_addrlen);
            if (result == 0){
                ready = 1;
                break;
            }
            if (errno == EINTR){
                LT_socket_interrupted();
                continue;
            }
            break;
        }
        if (ready && do_listen){
            ready = 0;
            while (1){
                if (listen(fd, backlog) == 0){
                    ready = 1;
                    break;
                }
                if (errno == EINTR){
                    LT_socket_interrupted();
                    continue;
                }
                break;
            }
        }
        if (ready){
            break;
        }
        saved_errno = errno;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(addresses);
    if (fd < 0){
        LT_system_error(
            passive ? "Could not bind socket" : "Could not connect socket",
            saved_errno
        );
    }
    return fd;
}

LT_UDPSocket* LT_UDPSocket_bind(const char* host, uint16_t port){
    LT_UDPSocket* result = LT_Class_ALLOC(LT_UDPSocket);
    set_fd(
        (LT_IPSocket*)result,
        resolve_socket(host, port, SOCK_DGRAM, 1, 0, 0)
    );
    return result;
}

LT_UDPSocket* LT_UDPSocket_connect(const char* host, uint16_t port){
    LT_UDPSocket* result = LT_Class_ALLOC(LT_UDPSocket);
    set_fd(
        (LT_IPSocket*)result,
        resolve_socket(host, port, SOCK_DGRAM, 0, 0, 0)
    );
    return result;
}
size_t LT_UDPSocket_send(LT_UDPSocket* socket, LT_ByteVector* bytes){
    ssize_t n;

    while (1){
        n = send(
            socket_fd((LT_IPSocket*)socket),
            LT_ByteVector_bytes(bytes),
            LT_ByteVector_length(bytes),
            0
        );
        if (n >= 0 || errno != EINTR){
            break;
        }
        LT_socket_interrupted();
    }
    if (n < 0){
        LT_system_error("Datagram send failed", errno);
    }
    if ((size_t)n != LT_ByteVector_length(bytes)){
        LT_error("Datagram send was incomplete");
    }
    return (size_t)n;
}
LT_ByteVector* LT_UDPSocket_receive(LT_UDPSocket* socket,
                                    size_t maximum_length){
    uint8_t* bytes = GC_MALLOC_ATOMIC(maximum_length ? maximum_length : 1);
    ssize_t n;
    while (1){
        n = recv(
            socket_fd((LT_IPSocket*)socket),
            bytes,
            maximum_length,
            0
        );
        if (n >= 0 || errno != EINTR){
            break;
        }
        LT_socket_interrupted();
    }
    if (n < 0){
        LT_system_error("Datagram receive failed", errno);
    }
    return LT_ByteVector_new(bytes, (size_t)n);
}

LT_TCPSocket* LT_TCPSocket_connect(const char* host, uint16_t port){
    LT_TCPSocket* result = LT_Class_ALLOC(LT_TCPSocket);
    set_fd(
        (LT_IPSocket*)result,
        resolve_socket(host, port, SOCK_STREAM, 0, 0, 0)
    );
    LT_socket_read_buffer_init(&result->read_buffer);
    return result;
}
static LT_TCPSocket* stream_from_fd(int fd){
    LT_TCPSocket* result = LT_Class_ALLOC(LT_TCPSocket);

    set_fd((LT_IPSocket*)result, fd);
    LT_socket_read_buffer_init(&result->read_buffer);
    return result;
}

size_t LT_TCPSocket_read(LT_TCPSocket* socket,
                         void* buffer,
                         size_t length){
    return LT_socket_buffered_read(
        socket_fd((LT_IPSocket*)socket),
        &socket->read_buffer,
        buffer,
        length,
        "Socket read failed"
    );
}

LT_Value LT_TCPSocket_readLine(LT_TCPSocket* socket){
    return LT_socket_buffered_read_line(
        socket_fd((LT_IPSocket*)socket),
        &socket->read_buffer,
        "Socket read failed"
    );
}
void LT_TCPSocket_write(LT_TCPSocket* socket,
                        const void* buffer,
                        size_t length){
    const uint8_t* bytes = buffer;
    size_t offset = 0;

    while (offset < length){
#ifdef MSG_NOSIGNAL
        ssize_t n = send(
            socket_fd((LT_IPSocket*)socket),
            bytes + offset,
            length - offset,
            MSG_NOSIGNAL
        );
#else
        ssize_t n = send(
            socket_fd((LT_IPSocket*)socket),
            bytes + offset,
            length - offset,
            0
        );
#endif
        if (n < 0 && errno == EINTR){
            LT_socket_interrupted();
            continue;
        }
        if (n <= 0){
            LT_system_error(
                "Socket write failed",
                n < 0 ? errno : EPIPE
            );
        }
        offset += (size_t)n;
    }
}
void LT_TCPSocket_shutdown_write(LT_TCPSocket* socket){
    if (shutdown(socket_fd((LT_IPSocket*)socket), SHUT_WR) != 0){
        LT_system_error("Socket shutdown failed", errno);
    }
}

LT_TCPServerSocket* LT_TCPServerSocket_new(const char* host,
                                           uint16_t port,
                                           int backlog){
    LT_TCPServerSocket* result = LT_Class_ALLOC(LT_TCPServerSocket);
    if (backlog < 1){
        LT_error("Socket backlog must be positive");
    }
    set_fd(
        (LT_IPSocket*)result,
        resolve_socket(host, port, SOCK_STREAM, 1, 1, backlog)
    );
    return result;
}
LT_TCPSocket* LT_TCPServerSocket_accept(LT_TCPServerSocket* socket){
    int fd;

    while (1){
        fd = accept(socket_fd((LT_IPSocket*)socket), NULL, NULL);
        if (fd >= 0 || errno != EINTR){
            break;
        }
        LT_socket_interrupted();
    }
    if (fd < 0){
        LT_system_error("Socket accept failed", errno);
    }
    return stream_from_fd(fd);
}

static LT_String* string_arg(LT_Value* cursor){
    LT_String* value;

    LT_GENERIC_ARG(*cursor, value, LT_String*, LT_String_from_value);
    return value;
}

LT_DEFINE_PRIMITIVE(
    socket_method_closed,
    "IPSocket>>isClosed",
    "(self)",
    "Return true when the socket is closed."
){
    LT_Value cursor = arguments;
    LT_IPSocket* self;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, self, LT_IPSocket*, socket_from_value);
    LT_ARG_END(cursor);
    return LT_IPSocket_closed(self) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    socket_method_close,
    "IPSocket>>close",
    "(self)",
    "Close the socket."
){
    LT_Value cursor = arguments;
    LT_IPSocket* self;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, self, LT_IPSocket*, socket_from_value);
    LT_ARG_END(cursor);
    LT_IPSocket_close(self);
    return (LT_Value)(uintptr_t)self;
}

LT_DEFINE_PRIMITIVE(
    datagram_bind,
    "UDPSocket class>>bindTo:port:",
    "(self host port)",
    "Bind a datagram socket."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value port;
    LT_String* host;

    (void)tail_call_unwind_marker;
    LT_OBJECT_ARG(cursor, self);
    host = string_arg(&cursor);
    LT_OBJECT_ARG(cursor, port);
    LT_ARG_END(cursor);
    (void)self;
    return (LT_Value)(uintptr_t)LT_UDPSocket_bind(
        LT_String_value_cstr(host),
        port_value(port)
    );
}

LT_DEFINE_PRIMITIVE(
    datagram_connect,
    "UDPSocket class>>connectTo:port:",
    "(self host port)",
    "Connect a datagram socket."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value port;
    LT_String* host;

    (void)tail_call_unwind_marker;
    LT_OBJECT_ARG(cursor, self);
    host = string_arg(&cursor);
    LT_OBJECT_ARG(cursor, port);
    LT_ARG_END(cursor);
    (void)self;
    return (LT_Value)(uintptr_t)LT_UDPSocket_connect(
        LT_String_value_cstr(host),
        port_value(port)
    );
}

LT_DEFINE_PRIMITIVE(
    datagram_send,
    "UDPSocket>>send:",
    "(self bytes)",
    "Send one datagram."
){
    LT_Value cursor = arguments;
    LT_UDPSocket* self;
    LT_ByteVector* bytes;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(
        cursor,
        self,
        LT_UDPSocket*,
        LT_UDPSocket_from_value
    );
    LT_GENERIC_ARG(cursor, bytes, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    return LT_Number_smallinteger_from_size(
        LT_UDPSocket_send(self, bytes),
        "Datagram too large"
    );
}

LT_DEFINE_PRIMITIVE(
    datagram_receive,
    "UDPSocket>>receive:",
    "(self maximumLength)",
    "Receive one datagram."
){
    LT_Value cursor = arguments;
    LT_Value maximum_length;
    LT_UDPSocket* self;
    size_t length;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(
        cursor,
        self,
        LT_UDPSocket*,
        LT_UDPSocket_from_value
    );
    LT_OBJECT_ARG(cursor, maximum_length);
    LT_ARG_END(cursor);
    length = LT_Number_nonnegative_size_from_integer(
        maximum_length,
        "Invalid receive length",
        "Invalid receive length"
    );
    return (LT_Value)(uintptr_t)LT_UDPSocket_receive(self, length);
}

LT_DEFINE_PRIMITIVE(
    stream_connect,
    "TCPSocket class>>connectTo:port:",
    "(self host port)",
    "Connect a stream socket."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value port;
    LT_String* host;

    (void)tail_call_unwind_marker;
    LT_OBJECT_ARG(cursor, self);
    host = string_arg(&cursor);
    LT_OBJECT_ARG(cursor, port);
    LT_ARG_END(cursor);
    (void)self;
    return (LT_Value)(uintptr_t)LT_TCPSocket_connect(
        LT_String_value_cstr(host),
        port_value(port)
    );
}

LT_DEFINE_PRIMITIVE(
    stream_read,
    "TCPSocket>>read:",
    "(self maximumLength)",
    "Read exactly maximumLength bytes, or fewer at end of stream."
){
    LT_Value cursor = arguments;
    LT_Value maximum_length;
    LT_TCPSocket* self;
    size_t length;
    size_t count;
    uint8_t* bytes;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(
        cursor,
        self,
        LT_TCPSocket*,
        LT_TCPSocket_from_value
    );
    LT_OBJECT_ARG(cursor, maximum_length);
    LT_ARG_END(cursor);
    length = LT_Number_nonnegative_size_from_integer(
        maximum_length,
        "Invalid read length",
        "Invalid read length"
    );
    bytes = GC_MALLOC_ATOMIC(length ? length : 1);
    count = LT_TCPSocket_read(self, bytes, length);
    return (LT_Value)(uintptr_t)LT_ByteVector_new(bytes, count);
}

LT_DEFINE_PRIMITIVE(
    stream_write,
    "TCPSocket>>write:",
    "(self bytes)",
    "Write all bytes."
){
    LT_Value cursor = arguments;
    LT_TCPSocket* self;
    LT_ByteVector* bytes;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(
        cursor,
        self,
        LT_TCPSocket*,
        LT_TCPSocket_from_value
    );
    LT_GENERIC_ARG(cursor, bytes, LT_ByteVector*, LT_ByteVector_from_value);
    LT_ARG_END(cursor);
    LT_TCPSocket_write(
        self,
        LT_ByteVector_bytes(bytes),
        LT_ByteVector_length(bytes)
    );
    return (LT_Value)(uintptr_t)self;
}

LT_DEFINE_PRIMITIVE(
    stream_read_line,
    "TCPSocket>>readLine",
    "(self)",
    "Read through a line feed and include it in the returned bytevector."
){
    LT_Value cursor = arguments;
    LT_TCPSocket* self;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(
        cursor,
        self,
        LT_TCPSocket*,
        LT_TCPSocket_from_value
    );
    LT_ARG_END(cursor);
    return LT_TCPSocket_readLine(self);
}

LT_DEFINE_PRIMITIVE(
    stream_shutdown,
    "TCPSocket>>shutdownWrite",
    "(self)",
    "Shut down the writing half."
){
    LT_Value cursor = arguments;
    LT_TCPSocket* self;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(
        cursor,
        self,
        LT_TCPSocket*,
        LT_TCPSocket_from_value
    );
    LT_ARG_END(cursor);
    LT_TCPSocket_shutdown_write(self);
    return (LT_Value)(uintptr_t)self;
}

LT_DEFINE_PRIMITIVE(
    server_new,
    "TCPServerSocket class>>newOn:port:backlog:",
    "(self host port backlog)",
    "Create a listening socket."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value port;
    LT_Value backlog_value;
    LT_String* host;
    size_t backlog;

    (void)tail_call_unwind_marker;
    LT_OBJECT_ARG(cursor, self);
    host = string_arg(&cursor);
    LT_OBJECT_ARG(cursor, port);
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
    return (LT_Value)(uintptr_t)LT_TCPServerSocket_new(
        LT_String_value_cstr(host),
        port_value(port),
        (int)backlog
    );
}

LT_DEFINE_PRIMITIVE(
    server_new_default_backlog,
    "TCPServerSocket class>>newOn:port:",
    "(self host port)",
    "Create a listening socket using the system maximum backlog."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value port;
    LT_String* host;

    (void)tail_call_unwind_marker;
    LT_OBJECT_ARG(cursor, self);
    host = string_arg(&cursor);
    LT_OBJECT_ARG(cursor, port);
    LT_ARG_END(cursor);
    (void)self;
    return (LT_Value)(uintptr_t)LT_TCPServerSocket_new(
        LT_String_value_cstr(host),
        port_value(port),
        SOMAXCONN
    );
}

LT_DEFINE_PRIMITIVE(
    server_accept,
    "TCPServerSocket>>accept",
    "(self)",
    "Accept and return a stream socket."
){
    LT_Value cursor = arguments;
    LT_TCPServerSocket* self;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(
        cursor,
        self,
        LT_TCPServerSocket*,
        LT_TCPServerSocket_from_value
    );
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_TCPServerSocket_accept(self);
}

static LT_Method_Descriptor ip_socket_methods[] = {
    {"isClosed", &socket_method_closed},
    {"close", &socket_method_close},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor datagram_methods[] = {
    {"send:", &datagram_send},
    {"receive:", &datagram_receive},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor datagram_class_methods[] = {
    {"bindTo:port:", &datagram_bind},
    {"connectTo:port:", &datagram_connect},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor stream_methods[] = {
    {"read:", &stream_read},
    {"readLine", &stream_read_line},
    {"write:", &stream_write},
    {"shutdownWrite", &stream_shutdown},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor stream_class_methods[] = {
    {"connectTo:port:", &stream_connect},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor server_methods[] = {
    {"accept", &server_accept},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor server_class_methods[] = {
    {"newOn:port:", &server_new_default_backlog},
    {"newOn:port:backlog:", &server_new},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Class* udp_socket_mixins[] = {
    &LT_DatagramSocket_class,
    NULL
};

static LT_Class* tcp_socket_mixins[] = {
    &LT_StreamSocket_class,
    NULL
};

static LT_Class* tcp_server_socket_mixins[] = {
    &LT_ServerSocket_class,
    NULL
};

LT_DEFINE_CLASS(LT_IPSocket) {
    .superclass = &LT_Socket_class,
    .metaclass_superclass = &LT_Class_class,
    .package = "ListTalk:Socket",
    .name = "IPSocket",
    .documentation = "Abstract socket implemented using the IP protocol suite.",
    .instance_size = sizeof(LT_IPSocket),
    .class_flags = LT_CLASS_FLAG_ABSTRACT,
    .methods = ip_socket_methods,
};

LT_DEFINE_CLASS(LT_UDPSocket) {
    .superclass = &LT_IPSocket_class,
    .mixins = udp_socket_mixins,
    .metaclass_superclass = &LT_Class_class,
    .package = "ListTalk:Socket",
    .name = "UDPSocket",
    .documentation = "UDP datagram socket.",
    .instance_size = sizeof(LT_UDPSocket),
    .methods = datagram_methods,
    .class_methods = datagram_class_methods,
};

LT_DEFINE_CLASS(LT_TCPSocket) {
    .superclass = &LT_IPSocket_class,
    .mixins = tcp_socket_mixins,
    .metaclass_superclass = &LT_Class_class,
    .package = "ListTalk:Socket",
    .name = "TCPSocket",
    .documentation = "Connected TCP byte-stream socket.",
    .instance_size = sizeof(LT_TCPSocket),
    .methods = stream_methods,
    .class_methods = stream_class_methods,
};

LT_DEFINE_CLASS(LT_TCPServerSocket) {
    .superclass = &LT_IPSocket_class,
    .mixins = tcp_server_socket_mixins,
    .metaclass_superclass = &LT_Class_class,
    .package = "ListTalk:Socket",
    .name = "TCPServerSocket",
    .documentation = "Listening TCP socket.",
    .instance_size = sizeof(LT_TCPServerSocket),
    .methods = server_methods,
    .class_methods = server_class_methods,
};
