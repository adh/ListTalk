/* SPDX-License-Identifier: MIT */
#include <ListTalk/ListTalk.h>
#include <ListTalk/networking/Socket.h>
#include <ListTalk/networking/HTTP.h>

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

static int failures;

typedef struct ShortWriteArguments {
    LT_UnixStreamSocket* socket;
} ShortWriteArguments;

static void* short_write(void* data){
    ShortWriteArguments* arguments = data;
    struct timespec delay = {
        .tv_sec = 0,
        .tv_nsec = 10000000,
    };

    LT_UnixStreamSocket_write(arguments->socket, "12", 2);
    nanosleep(&delay, NULL);
    LT_UnixStreamSocket_write(arguments->socket, "34", 2);
    return NULL;
}

static void check(int condition, const char* message){
    if (!condition){
        fprintf(stderr, "FAIL: %s\n", message);
        failures++;
    }
}

LT_DEFINE_PRIMITIVE(
    streaming_response_producer,
    "streaming-response-producer",
    "(response)",
    "Write a test streaming response."
){
    LT_Value cursor = arguments;
    LT_HTTPStreamingResponse* response;

    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(
        cursor,
        response,
        LT_HTTPStreamingResponse*,
        LT_HTTPStreamingResponse_from_value
    );
    LT_ARG_END(cursor);
    LT_HTTPStreamingResponse_set_status(response, 201);
    LT_HTTPStreamingResponse_header_at_put(
        response,
        LT_String_new_cstr("X-Test"),
        LT_String_new_cstr("streaming")
    );
    LT_HTTPStreamingResponse_write(
        response,
        (LT_Value)(uintptr_t)LT_String_new_cstr("hello")
    );
    return LT_NIL;
}

LT_DEFINE_PRIMITIVE(
    sse_response_producer,
    "sse-response-producer",
    "(response)",
    "Write a test server-sent event."
){
    LT_Value cursor = arguments;
    LT_HTTPStreamingResponse* response;

    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(
        cursor,
        response,
        LT_HTTPStreamingResponse*,
        LT_HTTPStreamingResponse_from_value
    );
    LT_ARG_END(cursor);
    LT_HTTPStreamingResponse_send_event_data(
        response,
        LT_String_new_cstr("update"),
        (LT_Value)(uintptr_t)LT_String_new_cstr("one\ntwo")
    );
    return LT_NIL;
}

static uint16_t local_port(LT_IPSocket* socket){
    struct sockaddr_in address;
    socklen_t length = sizeof(address);

    if (getsockname(
        LT_IPSocket_descriptor(socket),
        (struct sockaddr*)&address,
        &length
    ) != 0){
        return 0;
    }
    return ntohs(address.sin_port);
}

int main(void){
    LT_TCPServerSocket* server;
    LT_TCPServerSocket* wildcard_server;
    LT_TCPSocket *client, *peer;
    LT_UDPSocket *receiver, *sender;
    LT_UnixStreamSocket *unix_first, *unix_second;
    LT_UnixServerSocket* unix_server;
    LT_ByteVector *message, *received;
    LT_Value line;
    char reply[4];
    char unix_path[96];
    int option;
    socklen_t option_length;
    struct sockaddr_storage wildcard_address;
    socklen_t wildcard_address_length;
    ShortWriteArguments short_write_arguments;
    pthread_t writer;
    int writer_created;
    LT_Dictionary* headers;
    LT_Value header_value;
    LT_HTTPRequest* request;
    LT_ByteVector* body;
    char http_reply[256];
    size_t http_reply_length;

    LT_INIT();
    headers = LT_Dictionary_new();
    check(
        LT_HTTP_parse_header(headers,
                             (const uint8_t*)"Content-Type:  text/plain \r\n",
                             28) == 1,
        "HTTP header parser accepts a valid header"
    );
    check(
        LT_Dictionary_at(
            headers,
            (LT_Value)(uintptr_t)LT_String_new_cstr("content-type"),
            &header_value
        ) && LT_String_compare(
            LT_String_from_value(header_value),
            LT_String_new_cstr("text/plain")
        ) == 0,
        "HTTP header parser normalizes names and trims values"
    );
    check(
        LT_HTTP_parse_header(
            headers,
            (const uint8_t*)"Bad Header: x\r\n",
            15
        ) < 0,
        "HTTP header parser rejects invalid field names"
    );
    check(
        LT_HTTP_parse_header(headers, (const uint8_t*)"\r\n", 2) == 0,
        "HTTP header parser recognizes the header terminator"
    );
    wildcard_server = LT_TCPServerSocket_new("*", 0, SOMAXCONN);
    option = 0;
    option_length = sizeof(option);
    check(
        getsockopt(
            LT_IPSocket_descriptor((LT_IPSocket*)wildcard_server),
            SOL_SOCKET,
            SO_REUSEADDR,
            &option,
            &option_length
        ) == 0 && option != 0,
        "TCP wildcard server enables SO_REUSEADDR"
    );
    wildcard_address_length = sizeof(wildcard_address);
    check(
        getsockname(
            LT_IPSocket_descriptor((LT_IPSocket*)wildcard_server),
            (struct sockaddr*)&wildcard_address,
            &wildcard_address_length
        ) == 0,
        "TCP wildcard server has a local address"
    );
    if (wildcard_address.ss_family == AF_INET6){
        option = 1;
        option_length = sizeof(option);
        check(
            getsockopt(
                LT_IPSocket_descriptor((LT_IPSocket*)wildcard_server),
                IPPROTO_IPV6,
                IPV6_V6ONLY,
                &option,
                &option_length
            ) == 0 && option == 0,
            "TCP IPv6 server disables IPV6_V6ONLY"
        );
    }
    LT_IPSocket_close((LT_IPSocket*)wildcard_server);

    server = LT_TCPServerSocket_new("127.0.0.1", 0, 4);
    check(
        LT_Value_is_instance_of(
            (LT_Value)(uintptr_t)server,
            LT_STATIC_CLASS(LT_ServerSocket)
        ),
        "TCPServerSocket implements ServerSocket"
    );
    client = LT_TCPSocket_connect(
        "127.0.0.1",
        local_port((LT_IPSocket*)server)
    );
    peer = LT_TCPServerSocket_accept(server);
    check(
        LT_Value_is_instance_of(
            (LT_Value)(uintptr_t)peer,
            LT_STATIC_CLASS(LT_StreamSocket)
        ),
        "TCPSocket implements StreamSocket"
    );
    LT_TCPSocket_write(client, "ping", 4);
    check(
        LT_TCPSocket_read(peer, reply, sizeof(reply)) == 4
            && !memcmp(reply, "ping", 4),
        "stream socket loopback round trip"
    );
    LT_TCPSocket_write(
        client,
        "POST /items HTTP/1.1\r\nHost: example.test\r\nContent-Length: 4\r\n\r\ndata",
        sizeof("POST /items HTTP/1.1\r\nHost: example.test\r\nContent-Length: 4\r\n\r\ndata") - 1
    );
    request = LT_HTTPRequest_read(peer);
    check(
        LT_String_compare(LT_HTTPRequest_method(request),
                          LT_String_new_cstr("POST")) == 0
            && LT_String_compare(LT_HTTPRequest_target(request),
                                 LT_String_new_cstr("/items")) == 0,
        "HTTP request parser reads request line"
    );
    header_value = LT_HTTPRequest_header(request, LT_String_new_cstr("HOST"));
    check(
        header_value != LT_NIL
            && LT_String_compare(LT_String_from_value(header_value),
                                 LT_String_new_cstr("example.test")) == 0,
        "HTTP request header lookup is case insensitive"
    );
    body = LT_HTTPRequest_read_body(request);
    check(
        LT_ByteVector_length(body) == 4
            && !memcmp(LT_ByteVector_bytes(body), "data", 4),
        "HTTP request reads Content-Length body"
    );
    LT_HTTPRequest_respond_with(
        request,
        (LT_Value)(uintptr_t)LT_String_new_cstr("done")
    );
    LT_HTTPRequest_close(request);
    http_reply_length = LT_TCPSocket_read(client, http_reply, sizeof(http_reply));
    if (http_reply_length < sizeof(http_reply)){
        http_reply[http_reply_length] = '\0';
    }
    check(
        http_reply_length > 4 && http_reply_length < sizeof(http_reply)
            && strstr(http_reply, "HTTP/1.1 200 OK\r\n") == http_reply
            && strstr(
                http_reply,
                "Content-Type: text/plain; charset=utf-8\r\n"
            ) != NULL
            && strstr(http_reply, "Content-Length: 4\r\n") != NULL
            && !memcmp(http_reply + http_reply_length - 4, "done", 4),
        "HTTP response writer emits status, length, and body"
    );
    LT_IPSocket_close((LT_IPSocket*)client);

    client = LT_TCPSocket_connect(
        "127.0.0.1",
        local_port((LT_IPSocket*)server)
    );
    peer = LT_TCPServerSocket_accept(server);
    LT_TCPSocket_write(client, "GET /stream HTTP/1.1\r\n\r\n", 24);
    request = LT_HTTPRequest_read(peer);
    LT_HTTPRequest_respond_with(
        request,
        LT_Primitive_from_static(&streaming_response_producer)
    );
    http_reply_length = LT_TCPSocket_read(client, http_reply, sizeof(http_reply));
    if (http_reply_length < sizeof(http_reply)){
        http_reply[http_reply_length] = '\0';
    }
    check(
        http_reply_length > 5 && http_reply_length < sizeof(http_reply)
            && strstr(http_reply, "HTTP/1.1 201 Created\r\n") == http_reply
            && strstr(
                http_reply,
                "Content-Type: text/html; charset=utf-8\r\n"
            ) != NULL
            && strstr(http_reply, "X-Test: streaming\r\n") != NULL
            && strstr(http_reply, "Transfer-Encoding: chunked\r\n") != NULL
            && strstr(http_reply, "\r\n5\r\nhello\r\n0\r\n\r\n") != NULL,
        "StreamingResponse writes headers and chunked framing"
    );
    LT_IPSocket_close((LT_IPSocket*)client);

    client = LT_TCPSocket_connect(
        "127.0.0.1",
        local_port((LT_IPSocket*)server)
    );
    peer = LT_TCPServerSocket_accept(server);
    LT_TCPSocket_write(client, "GET /events HTTP/1.1\r\n\r\n", 24);
    request = LT_HTTPRequest_read(peer);
    LT_HTTPRequest_respond_with(
        request,
        LT_Primitive_from_static(&sse_response_producer)
    );
    http_reply_length = LT_TCPSocket_read(client, http_reply, sizeof(http_reply));
    if (http_reply_length < sizeof(http_reply)){
        http_reply[http_reply_length] = '\0';
    }
    check(
        http_reply_length > 5 && http_reply_length < sizeof(http_reply)
            && strstr(http_reply, "Content-Type: text/event-stream\r\n")
                != NULL
            && strstr(
                http_reply,
                "event: update\ndata: one\ndata: two\n\n"
            ) != NULL
            && strstr(http_reply, "\r\n0\r\n\r\n") != NULL,
        "StreamingResponse formats server-sent events as chunks"
    );
    LT_IPSocket_close((LT_IPSocket*)client);
    LT_IPSocket_close((LT_IPSocket*)server);
    check(
        LT_IPSocket_closed((LT_IPSocket*)server),
        "closed server reports closed"
    );

    receiver = LT_UDPSocket_bind("127.0.0.1", 0);
    check(
        LT_Value_is_instance_of(
            (LT_Value)(uintptr_t)receiver,
            LT_STATIC_CLASS(LT_DatagramSocket)
        ),
        "UDPSocket implements DatagramSocket"
    );
    sender = LT_UDPSocket_connect(
        "127.0.0.1",
        local_port((LT_IPSocket*)receiver)
    );
    message = LT_ByteVector_new((uint8_t*)"udp", 3);
    check(
        LT_UDPSocket_send(sender, message) == 3,
        "datagram send length"
    );
    received = LT_UDPSocket_receive(receiver, 16);
    check(
        LT_ByteVector_length(received) == 3
            && !memcmp(LT_ByteVector_bytes(received), "udp", 3),
        "datagram socket loopback receive"
    );
    LT_IPSocket_close((LT_IPSocket*)sender);
    LT_IPSocket_close((LT_IPSocket*)receiver);

    LT_UnixStreamSocket_pair(&unix_first, &unix_second);
    LT_UnixStreamSocket_write(unix_first, "pair", 4);
    check(
        LT_UnixStreamSocket_read(unix_second, reply, sizeof(reply)) == 4
            && !memcmp(reply, "pair", 4),
        "Unix stream socket pair round trip"
    );
    LT_UnixStreamSocket_write(unix_first, "a\nbc", 4);
    line = LT_UnixStreamSocket_readLine(unix_second);
    check(
        LT_ByteVector_p(line)
            && LT_ByteVector_length(LT_ByteVector_from_value(line)) == 2
            && !memcmp(
                LT_ByteVector_bytes(LT_ByteVector_from_value(line)),
                "a\n",
                2
            ),
        "Unix stream readLine includes the delimiter"
    );
    check(
        LT_UnixStreamSocket_read(unix_second, reply, 2) == 2
            && !memcmp(reply, "bc", 2),
        "Unix stream read consumes readLine buffered bytes"
    );
    short_write_arguments.socket = unix_first;
    writer_created = pthread_create(
        &writer,
        NULL,
        short_write,
        &short_write_arguments
    ) == 0;
    check(writer_created, "create short-write thread");
    if (writer_created){
        check(
            LT_UnixStreamSocket_read(unix_second, reply, 4) == 4
                && !memcmp(reply, "1234", 4),
            "high-level read shields caller from short reads"
        );
        pthread_join(writer, NULL);
    }
    check(
        LT_Value_is_instance_of(
            (LT_Value)(uintptr_t)unix_first,
            LT_STATIC_CLASS(LT_StreamSocket)
        ),
        "UnixStreamSocket implements StreamSocket"
    );
    LT_UnixSocket_close((LT_UnixSocket*)unix_first);
    LT_UnixSocket_close((LT_UnixSocket*)unix_second);

    snprintf(
        unix_path,
        sizeof(unix_path),
        "/tmp/listtalk-networking-test-%ld.sock",
        (long)getpid()
    );
    unlink(unix_path);
    unix_server = LT_UnixServerSocket_new(unix_path, 4);
    unix_first = LT_UnixStreamSocket_connect(unix_path);
    unix_second = LT_UnixServerSocket_accept(unix_server);
    LT_UnixStreamSocket_write(unix_first, "path", 4);
    check(
        LT_UnixStreamSocket_read(unix_second, reply, sizeof(reply)) == 4
            && !memcmp(reply, "path", 4),
        "pathname Unix stream socket round trip"
    );
    LT_UnixSocket_close((LT_UnixSocket*)unix_first);
    LT_UnixSocket_close((LT_UnixSocket*)unix_second);
    LT_UnixSocket_close((LT_UnixSocket*)unix_server);
    unlink(unix_path);
    return failures ? 1 : 0;
}
