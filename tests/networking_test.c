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
static LT_Value http_error_tag;
static uint16_t local_port(LT_IPSocket* socket);

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
    catch_http_error,
    "catch-http-error",
    "(condition)",
    "Capture an expected HTTP parser error."
){
    LT_Value cursor = arguments;
    LT_Value condition;

    (void)invocation_context_kind;
    (void)invocation_context_data;
    (void)tail_call_unwind_marker;
    LT_OBJECT_ARG(cursor, condition);
    LT_ARG_END(cursor);
    LT_throw(http_error_tag, condition);
}

static int request_fails_with(const char* request_bytes,
                              const LT_HTTPRequestLimits* limits,
                              const char* expected_message){
    LT_TCPServerSocket* listener = LT_TCPServerSocket_new("127.0.0.1", 0, 1);
    LT_TCPSocket* client = LT_TCPSocket_connect(
        "127.0.0.1",
        local_port((LT_IPSocket*)listener)
    );
    LT_TCPSocket* peer = LT_TCPServerSocket_accept(listener);
    LT_Value caught = LT_NIL;
    LT_Value handler = LT_Primitive_from_static(&catch_http_error);
    LT_Value message;

    LT_TCPSocket_write(client, request_bytes, strlen(request_bytes));
    http_error_tag = LT_Symbol_new("http-error-test-tag");
    LT_CATCH(http_error_tag, caught, {
        LT_HANDLER_BIND(handler, {
            (void)LT_HTTPRequest_read_with_limits(peer, limits);
        });
    });
    LT_IPSocket_close((LT_IPSocket*)peer);
    LT_IPSocket_close((LT_IPSocket*)client);
    LT_IPSocket_close((LT_IPSocket*)listener);
    if (caught == LT_NIL){
        return 0;
    }
    message = LT_Object_slot_ref(caught, LT_Symbol_new("message"));
    return LT_String_p(message)
        && !strcmp(
            LT_String_value_cstr(LT_String_from_value(message)),
            expected_message
        );
}

static int request_parses(const char* request_bytes,
                          const LT_HTTPRequestLimits* limits){
    LT_TCPServerSocket* listener = LT_TCPServerSocket_new("127.0.0.1", 0, 1);
    LT_TCPSocket* client = LT_TCPSocket_connect(
        "127.0.0.1",
        local_port((LT_IPSocket*)listener)
    );
    LT_TCPSocket* peer = LT_TCPServerSocket_accept(listener);
    LT_Value caught = LT_NIL;

    LT_TCPSocket_write(client, request_bytes, strlen(request_bytes));
    http_error_tag = LT_Symbol_new("http-error-test-tag");
    LT_CATCH(http_error_tag, caught, {
        LT_HANDLER_BIND(
            LT_Primitive_from_static(&catch_http_error),
            {
                (void)LT_HTTPRequest_read_with_limits(peer, limits);
            }
        );
    });
    LT_IPSocket_close((LT_IPSocket*)peer);
    LT_IPSocket_close((LT_IPSocket*)client);
    LT_IPSocket_close((LT_IPSocket*)listener);
    return caught == LT_NIL;
}

static int tcp_read_times_out(void){
    LT_TCPServerSocket* listener = LT_TCPServerSocket_new("127.0.0.1", 0, 1);
    LT_TCPSocket* client = LT_TCPSocket_connect(
        "127.0.0.1",
        local_port((LT_IPSocket*)listener)
    );
    LT_TCPSocket* peer = LT_TCPServerSocket_accept(listener);
    LT_Value caught = LT_NIL;
    int limit_exceeded;

    LT_TCPSocket_set_read_timeout_ms(peer, 1);
    http_error_tag = LT_Symbol_new("http-timeout-test-tag");
    LT_CATCH(http_error_tag, caught, {
        LT_HANDLER_BIND(
            LT_Primitive_from_static(&catch_http_error),
            {
                (void)LT_TCPSocket_readLineLimited(
                    peer,
                    64,
                    &limit_exceeded
                );
            }
        );
    });
    LT_IPSocket_close((LT_IPSocket*)peer);
    LT_IPSocket_close((LT_IPSocket*)client);
    LT_IPSocket_close((LT_IPSocket*)listener);
    return caught != LT_NIL;
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
        (LT_Value)(uintptr_t)LT_String_new_cstr("one\rtwo\r\nthree")
    );
    return LT_NIL;
}

LT_DEFINE_PRIMITIVE(
    streaming_bodyless_producer,
    "streaming-bodyless-producer",
    "(response)",
    "Write a body on a status that prohibits one."
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
    LT_HTTPStreamingResponse_set_status(response, 204);
    LT_HTTPStreamingResponse_write(
        response,
        (LT_Value)(uintptr_t)LT_String_new_cstr("not sent")
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
    LT_HTTPServer* http_server;
    LT_HTTPRequestLimits limits;
    LT_ByteVector* body;
    char http_reply[256];
    size_t http_reply_length;

    LT_INIT();
    http_server = LT_HTTPServer_new("127.0.0.1", 0, 1);
    check(
        LT_HTTPServer_request_line_length_limit(http_server)
            == LT_HTTP_DEFAULT_REQUEST_LINE_LENGTH_LIMIT
            && LT_HTTPServer_header_length_limit(http_server)
                == LT_HTTP_DEFAULT_HEADER_LENGTH_LIMIT
            && LT_HTTPServer_headers_size_limit(http_server)
                == LT_HTTP_DEFAULT_HEADERS_SIZE_LIMIT
            && LT_HTTPServer_request_body_size_limit(http_server)
                == LT_HTTP_DEFAULT_REQUEST_BODY_SIZE_LIMIT,
        "HTTP server initializes request size limits"
    );
    LT_HTTPServer_set_request_line_length_limit(http_server, 101);
    LT_HTTPServer_set_header_length_limit(http_server, 102);
    LT_HTTPServer_set_headers_size_limit(http_server, 103);
    LT_HTTPServer_set_request_body_size_limit(http_server, 104);
    check(
        LT_HTTPServer_request_line_length_limit(http_server) == 101
            && LT_HTTPServer_header_length_limit(http_server) == 102
            && LT_HTTPServer_headers_size_limit(http_server) == 103
            && LT_HTTPServer_request_body_size_limit(http_server) == 104,
        "HTTP server request size limits are configurable"
    );
    LT_HTTPServer_close(http_server);

    limits = (LT_HTTPRequestLimits){64, 64, 128, 64};
    limits.request_line_length = 8;
    check(
        request_fails_with(
            "GET / HTTP/1.1\r\n\r\n",
            &limits,
            "HTTP request line length limit exceeded"
        ),
        "HTTP parser enforces request line length limit"
    );
    limits = (LT_HTTPRequestLimits){64, 8, 128, 64};
    check(
        request_fails_with(
            "GET / HTTP/1.1\r\nX-Test: x\r\n\r\n",
            &limits,
            "HTTP header length limit exceeded"
        ),
        "HTTP parser enforces individual header length limit"
    );
    limits = (LT_HTTPRequestLimits){64, 64, 10, 64};
    check(
        request_fails_with(
            "GET / HTTP/1.1\r\nA: b\r\nC: d\r\n\r\n",
            &limits,
            "HTTP headers size limit exceeded"
        ),
        "HTTP parser enforces overall headers size limit"
    );
    limits = (LT_HTTPRequestLimits){64, 64, 128, 3};
    check(
        request_fails_with(
            "POST / HTTP/1.1\r\nContent-Length: 4\r\n\r\ndata",
            &limits,
            "HTTP request body size limit exceeded"
        ),
        "HTTP parser enforces request body size limit"
    );
    limits = (LT_HTTPRequestLimits){64, 64, 128, 64};
    check(
        request_fails_with(
            "POST / HTTP/1.1\r\nContent-Length: 4\r\n"
            "Content-Length: 5\r\n\r\n",
            &limits,
            "Malformed HTTP header"
        ),
        "HTTP parser rejects conflicting Content-Length fields"
    );
    check(
        request_parses(
            "POST / HTTP/1.1\r\nContent-Length: 4\r\n"
            "Content-Length: 4\r\n\r\n",
            &limits
        ),
        "HTTP parser coalesces identical Content-Length fields"
    );
    check(tcp_read_times_out(), "TCP socket read deadline expires");

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
    LT_TCPSocket_write(client, "GET /empty HTTP/1.1\r\n\r\n", 23);
    request = LT_HTTPRequest_read(peer);
    LT_HTTPRequest_respond(
        request,
        204,
        NULL,
        "not sent",
        sizeof("not sent") - 1
    );
    http_reply_length = LT_TCPSocket_read(client, http_reply, sizeof(http_reply));
    if (http_reply_length < sizeof(http_reply)){
        http_reply[http_reply_length] = '\0';
    }
    check(
        strstr(http_reply, "HTTP/1.1 204 No Content\r\n") == http_reply
            && strstr(http_reply, "Content-Length:") == NULL
            && strstr(http_reply, "not sent") == NULL,
        "HTTP response omits framing and body for bodyless statuses"
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
    LT_TCPSocket_write(client, "HEAD /stream HTTP/1.1\r\n\r\n", 25);
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
        strstr(http_reply, "HTTP/1.1 201 Created\r\n") == http_reply
            && strstr(http_reply, "Transfer-Encoding:") == NULL
            && strstr(http_reply, "hello") == NULL,
        "StreamingResponse suppresses framing and body for HEAD"
    );
    LT_IPSocket_close((LT_IPSocket*)client);

    client = LT_TCPSocket_connect(
        "127.0.0.1",
        local_port((LT_IPSocket*)server)
    );
    peer = LT_TCPServerSocket_accept(server);
    LT_TCPSocket_write(client, "GET /empty-stream HTTP/1.1\r\n\r\n", 30);
    request = LT_HTTPRequest_read(peer);
    LT_HTTPRequest_respond_with(
        request,
        LT_Primitive_from_static(&streaming_bodyless_producer)
    );
    http_reply_length = LT_TCPSocket_read(client, http_reply, sizeof(http_reply));
    if (http_reply_length < sizeof(http_reply)){
        http_reply[http_reply_length] = '\0';
    }
    check(
        strstr(http_reply, "HTTP/1.1 204 No Content\r\n") == http_reply
            && strstr(http_reply, "Transfer-Encoding:") == NULL
            && strstr(http_reply, "not sent") == NULL,
        "StreamingResponse suppresses framing and body for bodyless statuses"
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
                "event: update\ndata: one\ndata: two\ndata: three\n\n"
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
