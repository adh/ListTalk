/* SPDX-License-Identifier: MIT */
#include <ListTalk/ListTalk.h>
#include <ListTalk/networking/HTTP.h>

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>

struct LT_HTTPRequest_s {
    LT_Object base;
    LT_TCPSocket* socket;
    LT_String* method;
    LT_String* target;
    LT_String* version;
    LT_ImmutableDictionary* headers;
    size_t content_length;
    int body_read;
    int responded;
};

struct LT_HTTPServer_s {
    LT_Object base;
    LT_TCPServerSocket* socket;
};

struct LT_HTTPResponse_s {
    LT_Object base;
    unsigned status;
    LT_ImmutableDictionary* headers;
    LT_Value body;
};

struct LT_HTTPStreamingResponse_s {
    LT_Object base;
    LT_TCPSocket* socket;
    LT_Dictionary* headers;
    LT_String* content_type;
    unsigned status;
    int started;
    int closed;
};

static int token_character(uint8_t c){
    static const char separators[] = "()<>@,;:\\\"/[]?={} \t";
    return c > 32 && c < 127 && strchr(separators, c) == NULL;
}

LT_String* LT_HTTP_header_name(const char* bytes, size_t length){
    char* normalized = GC_MALLOC_ATOMIC(length ? length : 1);
    size_t i;
    for (i = 0; i < length; i++){
        normalized[i] = (char)tolower((unsigned char)bytes[i]);
    }
    return LT_String_new(normalized, length);
}

int LT_HTTP_parse_header(LT_Dictionary* headers,
                         const uint8_t* bytes,
                         size_t length){
    size_t colon = 0, start, end, i;
    LT_String* name;
    LT_String* value;

    while (length && (bytes[length - 1] == '\r' || bytes[length - 1] == '\n')){
        length--;
    }
    if (!length){
        return 0;
    }
    while (colon < length && bytes[colon] != ':'){
        colon++;
    }
    if (!colon || colon == length){
        return -1;
    }
    for (i = 0; i < colon; i++){
        if (!token_character(bytes[i])){
            return -1;
        }
    }
    start = colon + 1;
    while (start < length && (bytes[start] == ' ' || bytes[start] == '\t')){
        start++;
    }
    end = length;
    while (end > start
            && (bytes[end - 1] == ' ' || bytes[end - 1] == '\t')){
        end--;
    }
    for (i = start; i < end; i++){
        if ((bytes[i] < 32 && bytes[i] != '\t') || bytes[i] == 127){
            return -1;
        }
    }
    name = LT_HTTP_header_name((const char*)bytes, colon);
    value = LT_String_new((char*)bytes + start, end - start);
    LT_Dictionary_atPut(headers, (LT_Value)(uintptr_t)name,
                        (LT_Value)(uintptr_t)value);
    return 1;
}

static LT_ByteVector* line_bytes(LT_Value line){
    if (line == LT_NIL){
        LT_error("Unexpected end of HTTP request");
    }
    return LT_ByteVector_from_value(line);
}

static void parse_request_line(LT_HTTPRequest* request, LT_ByteVector* line){
    const uint8_t* bytes = LT_ByteVector_bytes(line);
    size_t length = LT_ByteVector_length(line);
    size_t first = 0;
    size_t second;

    while (length
            && (bytes[length - 1] == '\r' || bytes[length - 1] == '\n')){
        length--;
    }
    while (first < length && bytes[first] != ' '){
        first++;
    }
    second = first + 1;
    while (second < length && bytes[second] != ' '){
        second++;
    }
    if (!first || second <= first + 1 || second == length
            || memchr(bytes + second + 1, ' ', length - second - 1)){
        LT_error("Malformed HTTP request line");
    }
    request->method = LT_String_new((char*)bytes, first);
    request->target = LT_String_new(
        (char*)bytes + first + 1,
        second - first - 1
    );
    request->version = LT_String_new(
        (char*)bytes + second + 1,
        length - second - 1
    );
    if (strncmp(LT_String_value_cstr(request->version), "HTTP/", 5) != 0){
        LT_error("Malformed HTTP version");
    }
}

static size_t content_length(LT_Dictionary* headers){
    LT_Value value;
    LT_String* key = LT_HTTP_header_name("content-length", 14);
    char* end;
    unsigned long long result;
    if (!LT_Dictionary_at(headers, (LT_Value)(uintptr_t)key, &value)){
        return 0;
    }
    if (!LT_String_byte_length(LT_String_from_value(value))){
        LT_error("Invalid HTTP Content-Length");
    }
    result = strtoull(
        LT_String_value_cstr(LT_String_from_value(value)),
        &end,
        10
    );
    if (*end || result > SIZE_MAX
            || end == LT_String_value_cstr(LT_String_from_value(value))){
        LT_error("Invalid HTTP Content-Length");
    }
    return (size_t)result;
}

LT_HTTPRequest* LT_HTTPRequest_read(LT_TCPSocket* socket){
    LT_HTTPRequest* request = LT_Class_ALLOC(LT_HTTPRequest);
    LT_Dictionary* headers = LT_Dictionary_new();
    LT_ByteVector* line;
    int result;
    request->socket = socket;
    request->body_read = request->responded = 0;
    parse_request_line(request, line_bytes(LT_TCPSocket_readLine(socket)));
    for (;;){
        line = line_bytes(LT_TCPSocket_readLine(socket));
        result = LT_HTTP_parse_header(headers, LT_ByteVector_bytes(line),
                                      LT_ByteVector_length(line));
        if (!result){
            break;
        }
        if (result < 0){
            LT_error("Malformed HTTP header");
        }
    }
    request->headers = (LT_ImmutableDictionary*)headers;
    request->content_length = content_length(headers);
    return request;
}

LT_String* LT_HTTPRequest_method(LT_HTTPRequest* request){
    return request->method;
}

LT_String* LT_HTTPRequest_target(LT_HTTPRequest* request){
    return request->target;
}

LT_String* LT_HTTPRequest_version(LT_HTTPRequest* request){
    return request->version;
}

LT_ImmutableDictionary* LT_HTTPRequest_headers(LT_HTTPRequest* request){
    return request->headers;
}

LT_Value LT_HTTPRequest_header(LT_HTTPRequest* request, LT_String* name){
    LT_Value value;
    LT_String* key = LT_HTTP_header_name(LT_String_value_cstr(name),
                                         LT_String_byte_length(name));
    return LT_Dictionary_at(
        (LT_Dictionary*)request->headers,
        (LT_Value)(uintptr_t)key,
        &value
    ) ? value : LT_NIL;
}

LT_ByteVector* LT_HTTPRequest_read_body(LT_HTTPRequest* request){
    uint8_t* bytes;
    size_t count;
    if (request->body_read){
        LT_error("HTTP request body was already read");
    }
    request->body_read = 1;
    bytes = GC_MALLOC_ATOMIC(
        request->content_length ? request->content_length : 1
    );
    count = LT_TCPSocket_read(request->socket, bytes, request->content_length);
    if (count != request->content_length){
        LT_error("Unexpected end of HTTP request body");
    }
    return LT_ByteVector_new(bytes, count);
}

static const char* reason_phrase(unsigned status){
    switch (status){
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default: return "";
    }
}

void LT_HTTPRequest_respond(LT_HTTPRequest* request, unsigned status,
                            LT_Dictionary* headers, const void* body,
                            size_t body_length){
    char first[96];
    char length_header[64];
    LT_Value entries;
    int n;

    if (request->responded){
        LT_error("HTTP request was already answered");
    }
    if (status < 100 || status > 999){
        LT_error("Invalid HTTP response status");
    }
    request->responded = 1;
    n = snprintf(first, sizeof(first), "HTTP/1.1 %u %s\r\n", status,
                 reason_phrase(status));
    LT_TCPSocket_write(request->socket, first, (size_t)n);
    entries = headers ? LT_Dictionary_asAList(headers) : LT_NIL;
    while (entries != LT_NIL){
        LT_Value entry = LT_car(entries);
        LT_String* name = LT_String_from_value(LT_car(entry));
        LT_String* value = LT_String_from_value(LT_cdr(entry));
        const char* name_bytes = LT_String_value_cstr(name);
        const char* value_bytes = LT_String_value_cstr(value);
        size_t name_length = LT_String_byte_length(name);
        size_t value_length = LT_String_byte_length(value);
        size_t i;

        if (!name_length){
            LT_error("Invalid HTTP response header name");
        }
        if ((name_length == 14 && !strncasecmp(name_bytes, "content-length", 14))
                || (name_length == 10
                    && !strncasecmp(name_bytes, "connection", 10))
                || (name_length == 17
                    && !strncasecmp(name_bytes, "transfer-encoding", 17))){
            LT_error("HTTP response framing headers are managed by the server");
        }
        for (i = 0; i < name_length; i++){
            if (!token_character((uint8_t)name_bytes[i])){
                LT_error("Invalid HTTP response header name");
            }
        }
        for (i = 0; i < value_length; i++){
            if (value_bytes[i] == '\r' || value_bytes[i] == '\n'){
                LT_error("Invalid HTTP response header value");
            }
        }
        LT_TCPSocket_write(
            request->socket,
            LT_String_value_cstr(name),
            LT_String_byte_length(name)
        );
        LT_TCPSocket_write(request->socket, ": ", 2);
        LT_TCPSocket_write(
            request->socket,
            LT_String_value_cstr(value),
            LT_String_byte_length(value)
        );
        LT_TCPSocket_write(request->socket, "\r\n", 2);
        entries = LT_cdr(entries);
    }
    n = snprintf(length_header, sizeof(length_header),
                 "Content-Length: %zu\r\nConnection: close\r\n\r\n", body_length);
    LT_TCPSocket_write(request->socket, length_header, (size_t)n);
    if (body_length){
        LT_TCPSocket_write(request->socket, body, body_length);
    }
    LT_IPSocket_close((LT_IPSocket*)request->socket);
}

void LT_HTTPRequest_close(LT_HTTPRequest* request){
    LT_IPSocket_close((LT_IPSocket*)request->socket);
}

LT_HTTPResponse* LT_HTTPResponse_new(unsigned status,
                                     LT_Dictionary* headers,
                                     LT_Value body){
    LT_HTTPResponse* response;

    if (status < 100 || status > 999){
        LT_error("Invalid HTTP response status");
    }
    if (!LT_String_p(body) && !LT_ByteVector_p(body)){
        LT_error("HTTP response body must be a String or ByteVector");
    }
    response = LT_Class_ALLOC(LT_HTTPResponse);
    response->status = status;
    response->headers = headers
        ? (LT_ImmutableDictionary*)headers
        : LT_ImmutableDictionary_new();
    response->body = body;
    return response;
}

unsigned LT_HTTPResponse_status(LT_HTTPResponse* response){
    return response->status;
}

LT_ImmutableDictionary* LT_HTTPResponse_headers(LT_HTTPResponse* response){
    return response->headers;
}

LT_Value LT_HTTPResponse_body(LT_HTTPResponse* response){
    return response->body;
}

static void response_data(LT_Value body, const void** bytes, size_t* length){
    if (LT_String_p(body)){
        LT_String* string = LT_String_from_value(body);
        *bytes = LT_String_value_cstr(string);
        *length = LT_String_byte_length(string);
    } else {
        LT_ByteVector* bytevector = LT_ByteVector_from_value(body);
        *bytes = LT_ByteVector_bytes(bytevector);
        *length = LT_ByteVector_length(bytevector);
    }
}

static void streaming_response_configurable(
    LT_HTTPStreamingResponse* response
){
    if (response->started){
        LT_error("Streaming response has already started");
    }
}

static int reserved_streaming_header(LT_String* name){
    const char* bytes = LT_String_value_cstr(name);
    size_t length = LT_String_byte_length(name);

#define HEADER_IS(header)                                                    \
    (length == sizeof(header) - 1                                            \
        && !strncasecmp(bytes, header, sizeof(header) - 1))
    return HEADER_IS("content-type")
        || HEADER_IS("content-length")
        || HEADER_IS("connection")
        || HEADER_IS("transfer-encoding");
#undef HEADER_IS
}

void LT_HTTPStreamingResponse_set_status(LT_HTTPStreamingResponse* response,
                                         unsigned status){
    streaming_response_configurable(response);
    if (status < 100 || status > 999){
        LT_error("Invalid HTTP response status");
    }
    response->status = status;
}

void LT_HTTPStreamingResponse_set_content_type(
    LT_HTTPStreamingResponse* response,
    LT_String* content_type
){
    const char* bytes = LT_String_value_cstr(content_type);
    size_t length = LT_String_byte_length(content_type);

    streaming_response_configurable(response);
    if (!length || memchr(bytes, '\r', length) || memchr(bytes, '\n', length)){
        LT_error("Invalid HTTP Content-Type");
    }
    response->content_type = content_type;
}

void LT_HTTPStreamingResponse_header_at_put(
    LT_HTTPStreamingResponse* response,
    LT_String* name,
    LT_String* value
){
    const char* name_bytes = LT_String_value_cstr(name);
    const char* value_bytes = LT_String_value_cstr(value);
    size_t name_length = LT_String_byte_length(name);
    size_t value_length = LT_String_byte_length(value);
    size_t i;

    streaming_response_configurable(response);
    if (!name_length || reserved_streaming_header(name)){
        LT_error("Header is managed by StreamingResponse");
    }
    for (i = 0; i < name_length; i++){
        if (!token_character((uint8_t)name_bytes[i])){
            LT_error("Invalid HTTP response header name");
        }
    }
    for (i = 0; i < value_length; i++){
        if (value_bytes[i] == '\r' || value_bytes[i] == '\n'){
            LT_error("Invalid HTTP response header value");
        }
    }
    LT_Dictionary_atPut(
        response->headers,
        (LT_Value)(uintptr_t)name,
        (LT_Value)(uintptr_t)value
    );
}

static void streaming_response_start(LT_HTTPStreamingResponse* response,
                                     const char* default_content_type){
    static const char framing_headers[] =
        "Transfer-Encoding: chunked\r\n"
        "Connection: close\r\n\r\n";
    char status_line[96];
    LT_Value entries;
    int length;

    if (response->started){
        return;
    }
    response->started = 1;
    length = snprintf(
        status_line,
        sizeof(status_line),
        "HTTP/1.1 %u %s\r\n",
        response->status,
        reason_phrase(response->status)
    );
    LT_TCPSocket_write(response->socket, status_line, (size_t)length);
    LT_TCPSocket_write(response->socket, "Content-Type: ", 14);
    if (response->content_type){
        LT_TCPSocket_write(
            response->socket,
            LT_String_value_cstr(response->content_type),
            LT_String_byte_length(response->content_type)
        );
    } else {
        LT_TCPSocket_write(
            response->socket,
            default_content_type,
            strlen(default_content_type)
        );
    }
    LT_TCPSocket_write(response->socket, "\r\n", 2);
    entries = LT_Dictionary_asAList(response->headers);
    while (entries != LT_NIL){
        LT_Value entry = LT_car(entries);
        LT_String* name = LT_String_from_value(LT_car(entry));
        LT_String* value = LT_String_from_value(LT_cdr(entry));

        LT_TCPSocket_write(
            response->socket,
            LT_String_value_cstr(name),
            LT_String_byte_length(name)
        );
        LT_TCPSocket_write(response->socket, ": ", 2);
        LT_TCPSocket_write(
            response->socket,
            LT_String_value_cstr(value),
            LT_String_byte_length(value)
        );
        LT_TCPSocket_write(response->socket, "\r\n", 2);
        entries = LT_cdr(entries);
    }
    LT_TCPSocket_write(
        response->socket,
        framing_headers,
        sizeof(framing_headers) - 1
    );
}

static void streaming_response_chunk(LT_HTTPStreamingResponse* response,
                                     const void* bytes,
                                     size_t length){
    char prefix[2 * sizeof(size_t) + 3];
    int prefix_length;

    if (response->closed){
        LT_error("Streaming response is closed");
    }
    if (!length){
        return;
    }
    prefix_length = snprintf(prefix, sizeof(prefix), "%zx\r\n", length);
    LT_TCPSocket_write(response->socket, prefix, (size_t)prefix_length);
    LT_TCPSocket_write(response->socket, bytes, length);
    LT_TCPSocket_write(response->socket, "\r\n", 2);
}

void LT_HTTPStreamingResponse_write(LT_HTTPStreamingResponse* response,
                                    LT_Value data){
    const void* bytes;
    size_t length;

    if (!LT_String_p(data) && !LT_ByteVector_p(data)){
        LT_error("Streaming response data must be a String or ByteVector");
    }
    streaming_response_start(
        response,
        LT_String_p(data) ? "text/html; charset=utf-8" : "application/octet-stream"
    );
    response_data(data, &bytes, &length);
    streaming_response_chunk(response, bytes, length);
}

static void streaming_response_sse_field(LT_StringBuilder* event,
                                         LT_String* field,
                                         LT_Value value){
    const void* bytes;
    size_t length;
    const uint8_t* data;
    size_t start = 0;
    size_t i;

    for (i = 0; i < LT_String_byte_length(field); i++){
        char character = LT_String_value_cstr(field)[i];

        if (character == ':' || character == '\r' || character == '\n'){
            LT_error("Invalid SSE field name");
        }
    }
    if (!LT_String_p(value) && !LT_ByteVector_p(value)){
        LT_error("SSE field value must be a String or ByteVector");
    }
    response_data(value, &bytes, &length);
    data = bytes;
    for (i = 0; i <= length; i++){
        if (i == length || data[i] == '\n'){
            size_t end = i;

            if (end > start && data[end - 1] == '\r'){
                end--;
            }
            LT_StringBuilder_append_bytes(
                event,
                LT_String_value_cstr(field),
                LT_String_byte_length(field)
            );
            LT_StringBuilder_append_bytes(event, ": ", 2);
            LT_StringBuilder_append_bytes(
                event,
                (const char*)data + start,
                end - start
            );
            LT_StringBuilder_append_char(event, '\n');
            start = i + 1;
        }
    }
}

void LT_HTTPStreamingResponse_send_data(LT_HTTPStreamingResponse* response,
                                        LT_Value data){
    LT_StringBuilder* event = LT_StringBuilder_new();

    streaming_response_start(response, "text/event-stream");
    streaming_response_sse_field(
        event,
        LT_String_new_cstr("data"),
        data
    );
    LT_StringBuilder_append_char(event, '\n');
    streaming_response_chunk(
        response,
        LT_StringBuilder_value(event),
        LT_StringBuilder_length(event)
    );
}

void LT_HTTPStreamingResponse_send_event_data(
    LT_HTTPStreamingResponse* response,
    LT_String* event,
    LT_Value data
){
    LT_StringBuilder* fields = LT_StringBuilder_new();

    streaming_response_start(response, "text/event-stream");
    streaming_response_sse_field(
        fields,
        LT_String_new_cstr("event"),
        (LT_Value)(uintptr_t)event
    );
    streaming_response_sse_field(
        fields,
        LT_String_new_cstr("data"),
        data
    );
    LT_StringBuilder_append_char(fields, '\n');
    streaming_response_chunk(
        response,
        LT_StringBuilder_value(fields),
        LT_StringBuilder_length(fields)
    );
}

void LT_HTTPStreamingResponse_send_event_with_fields(
    LT_HTTPStreamingResponse* response,
    LT_Value fields
){
    LT_Value entries = LT_List_p(fields) ? fields : LT_SEND(fields, "asAList");
    LT_StringBuilder* event = LT_StringBuilder_new();

    streaming_response_start(response, "text/event-stream");
    while (entries != LT_NIL){
        LT_Value entry = LT_car(entries);
        LT_String* field = LT_String_from_value(LT_car(entry));

        streaming_response_sse_field(event, field, LT_cdr(entry));
        entries = LT_cdr(entries);
    }
    LT_StringBuilder_append_char(event, '\n');
    streaming_response_chunk(
        response,
        LT_StringBuilder_value(event),
        LT_StringBuilder_length(event)
    );
}

static void streaming_response_finish(LT_HTTPStreamingResponse* response){
    if (response->closed){
        return;
    }
    streaming_response_start(response, "text/html; charset=utf-8");
    LT_TCPSocket_write(response->socket, "0\r\n\r\n", 5);
    response->closed = 1;
    LT_IPSocket_close((LT_IPSocket*)response->socket);
}

void LT_HTTPRequest_respond_with(LT_HTTPRequest* request, LT_Value result){
    const void* bytes;
    size_t length;

    if (LT_String_p(result) || LT_ByteVector_p(result)){
        LT_Dictionary* headers = LT_Dictionary_new();
        LT_Dictionary_atPut(
            headers,
            (LT_Value)(uintptr_t)LT_String_new_cstr("Content-Type"),
            (LT_Value)(uintptr_t)LT_String_new_cstr(
                LT_String_p(result)
                    ? "text/plain; charset=utf-8"
                    : "application/octet-stream"
            )
        );
        response_data(result, &bytes, &length);
        LT_HTTPRequest_respond(request, 200, headers, bytes, length);
        return;
    }
    if (LT_Value_is_instance_of(result, LT_STATIC_CLASS(LT_HTTPResponse))){
        LT_HTTPResponse* response = LT_HTTPResponse_from_value(result);
        response_data(response->body, &bytes, &length);
        LT_HTTPRequest_respond(request, response->status,
                               (LT_Dictionary*)response->headers,
                               bytes, length);
        return;
    }
    if (LT_Value_is_instance_of(result, LT_STATIC_CLASS(LT_Function))){
        LT_HTTPStreamingResponse* response = LT_Class_ALLOC(
            LT_HTTPStreamingResponse
        );

        response->socket = request->socket;
        response->headers = LT_Dictionary_new();
        response->content_type = NULL;
        response->status = 200;
        response->started = 0;
        response->closed = 0;
        request->responded = 1;
        (void)LT_apply(
            result,
            LT_cons((LT_Value)(uintptr_t)response, LT_NIL),
            LT_NIL,
            LT_NIL,
            NULL
        );
        streaming_response_finish(response);
        return;
    }
    LT_error(
        "HTTP handler must return a String, ByteVector, Response, "
        "or SSE lambda"
    );
}

LT_HTTPServer* LT_HTTPServer_new(const char* host, uint16_t port, int backlog){
    LT_HTTPServer* result = LT_Class_ALLOC(LT_HTTPServer);

    result->socket = LT_TCPServerSocket_new(host, port, backlog);
    return result;
}

LT_HTTPRequest* LT_HTTPServer_accept(LT_HTTPServer* server){
    return LT_HTTPRequest_read(LT_TCPServerSocket_accept(server->socket));
}

LT_Value LT_HTTPServer_handle(LT_HTTPServer* server, LT_Value handler){
    LT_HTTPRequest* request;
    LT_Value result;

    request = LT_HTTPServer_accept(server);
    result = LT_apply(
        handler,
        LT_cons((LT_Value)(uintptr_t)request, LT_NIL),
        LT_NIL, LT_NIL, NULL
    );
    LT_HTTPRequest_respond_with(request, result);
    return result;
}

void LT_HTTPServer_close(LT_HTTPServer* server){
    LT_IPSocket_close((LT_IPSocket*)server->socket);
}

/* Language-facing methods live here so the reusable classes remain in the
 * library. */
static LT_HTTPRequest* request_arg(LT_Value* cursor){
    LT_HTTPRequest* request;

    LT_GENERIC_ARG(
        *cursor,
        request,
        LT_HTTPRequest*,
        LT_HTTPRequest_from_value
    );
    return request;
}

#define REQUEST_ACCESSOR(c_name, selector, field, documentation)             \
    LT_DEFINE_PRIMITIVE(                                                     \
        c_name,                                                              \
        "HTTPRequest>>" selector,                                           \
        "(self)",                                                           \
        documentation                                                       \
    ){                                                                       \
        LT_Value cursor = arguments;                                         \
        LT_HTTPRequest* self;                                                \
                                                                             \
        (void)tail_call_unwind_marker;                                       \
        self = request_arg(&cursor);                                         \
        LT_ARG_END(cursor);                                                  \
        return (LT_Value)(uintptr_t)self->field;                             \
    }

REQUEST_ACCESSOR(req_method, "method", method, "Return the request method.")
REQUEST_ACCESSOR(req_target, "target", target, "Return the request target.")
REQUEST_ACCESSOR(req_version, "version", version, "Return the HTTP version.")
REQUEST_ACCESSOR(req_headers, "headers", headers, "Return the request headers.")

LT_DEFINE_PRIMITIVE(
    req_header,
    "HTTPRequest>>headerAt:",
    "(self name)",
    "Look up a header case-insensitively."
){
    LT_Value cursor = arguments;
    LT_HTTPRequest* self;
    LT_String* name;

    (void)tail_call_unwind_marker;
    self = request_arg(&cursor);
    LT_GENERIC_ARG(cursor, name, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);
    return LT_HTTPRequest_header(self, name);
}

LT_DEFINE_PRIMITIVE(
    req_body,
    "HTTPRequest>>body",
    "(self)",
    "Read and return the request body."
){
    LT_Value cursor = arguments;
    LT_HTTPRequest* self;

    (void)tail_call_unwind_marker;
    self = request_arg(&cursor);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_HTTPRequest_read_body(self);
}

static unsigned response_status_arg(LT_Value value){
    size_t status = LT_Number_nonnegative_size_from_integer(
        value,
        "Invalid HTTP status",
        "Invalid HTTP status"
    );

    if (status < 100 || status > 999){
        LT_error("Invalid HTTP status");
    }
    return (unsigned)status;
}

LT_DEFINE_PRIMITIVE(
    req_respond,
    "HTTPRequest>>respond:body:",
    "(self status body)",
    "Send an HTTP response and return the receiver."
){
    LT_Value cursor = arguments;
    LT_Value status;
    LT_Value body;
    LT_HTTPRequest* self;
    const void* bytes;
    size_t length;

    (void)tail_call_unwind_marker;
    self = request_arg(&cursor);
    LT_OBJECT_ARG(cursor, status);
    LT_OBJECT_ARG(cursor, body);
    LT_ARG_END(cursor);
    response_data(body, &bytes, &length);
    LT_HTTPRequest_respond(
        self,
        response_status_arg(status),
        NULL,
        bytes,
        length
    );
    return (LT_Value)(uintptr_t)self;
}

LT_DEFINE_PRIMITIVE(
    req_respond_headers,
    "HTTPRequest>>respond:headers:body:",
    "(self status headers body)",
    "Send an HTTP response with headers and return the receiver."
){
    LT_Value cursor = arguments;
    LT_Value status;
    LT_Value body;
    LT_HTTPRequest* self;
    LT_Dictionary* headers;
    const void* bytes;
    size_t length;

    (void)tail_call_unwind_marker;
    self = request_arg(&cursor);
    LT_OBJECT_ARG(cursor, status);
    LT_GENERIC_ARG(cursor, headers, LT_Dictionary*, LT_Dictionary_from_value);
    LT_OBJECT_ARG(cursor, body);
    LT_ARG_END(cursor);
    response_data(body, &bytes, &length);
    LT_HTTPRequest_respond(
        self,
        response_status_arg(status),
        headers,
        bytes,
        length
    );
    return (LT_Value)(uintptr_t)self;
}

LT_DEFINE_PRIMITIVE(
    req_close,
    "HTTPRequest>>close",
    "(self)",
    "Close the request connection."
){
    LT_Value cursor = arguments;
    LT_HTTPRequest* self;

    (void)tail_call_unwind_marker;
    self = request_arg(&cursor);
    LT_ARG_END(cursor);
    LT_HTTPRequest_close(self);
    return (LT_Value)(uintptr_t)self;
}

LT_DEFINE_PRIMITIVE(
    server_new,
    "HTTPServer class>>newOn:port:",
    "(self host port)",
    "Create an HTTP server."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value port;
    LT_String* host;
    size_t port_number;

    (void)tail_call_unwind_marker;
    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, host, LT_String*, LT_String_from_value);
    LT_OBJECT_ARG(cursor, port);
    LT_ARG_END(cursor);
    (void)self;
    port_number = LT_Number_nonnegative_size_from_integer(
        port,
        "Invalid HTTP port",
        "Invalid HTTP port"
    );
    if (port_number > UINT16_MAX){
        LT_error("Invalid HTTP port");
    }
    return (LT_Value)(uintptr_t)LT_HTTPServer_new(
        LT_String_value_cstr(host),
        (uint16_t)port_number,
        SOMAXCONN
    );
}

LT_DEFINE_PRIMITIVE(
    server_accept,
    "HTTPServer>>accept",
    "(self)",
    "Accept and parse one HTTP request."
){
    LT_Value cursor = arguments;
    LT_HTTPServer* self;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, self, LT_HTTPServer*, LT_HTTPServer_from_value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_HTTPServer_accept(self);
}

LT_DEFINE_PRIMITIVE(
    server_handle,
    "HTTPServer>>handle:",
    "(self handler)",
    "Handle one request with handler and serialize its result."
){
    LT_Value cursor = arguments;
    LT_Value handler;
    LT_HTTPServer* self;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, self, LT_HTTPServer*, LT_HTTPServer_from_value);
    LT_OBJECT_ARG(cursor, handler);
    LT_ARG_END(cursor);
    return LT_HTTPServer_handle(self, handler);
}

LT_DEFINE_PRIMITIVE(
    server_serve,
    "HTTPServer>>serve:",
    "(self handler)",
    "Handle requests until the server is closed or an error is signaled."
){
    LT_Value cursor = arguments;
    LT_Value handler;
    LT_HTTPServer* self;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, self, LT_HTTPServer*, LT_HTTPServer_from_value);
    LT_OBJECT_ARG(cursor, handler);
    LT_ARG_END(cursor);
    for (;;){
        (void)LT_HTTPServer_handle(self, handler);
    }
    return LT_NIL;
}

LT_DEFINE_PRIMITIVE(
    server_close,
    "HTTPServer>>close",
    "(self)",
    "Close the HTTP server."
){
    LT_Value cursor = arguments;
    LT_HTTPServer* self;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, self, LT_HTTPServer*, LT_HTTPServer_from_value);
    LT_ARG_END(cursor);
    LT_HTTPServer_close(self);
    return (LT_Value)(uintptr_t)self;
}

LT_DEFINE_PRIMITIVE(
    response_new,
    "Response class>>new:headers:body:",
    "(self status headers body)",
    "Create an HTTP response."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value status;
    LT_Value body;
    LT_Dictionary* headers;

    (void)tail_call_unwind_marker;
    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, status);
    LT_GENERIC_ARG(cursor, headers, LT_Dictionary*, LT_Dictionary_from_value);
    LT_OBJECT_ARG(cursor, body);
    LT_ARG_END(cursor);
    (void)self;
    return (LT_Value)(uintptr_t)LT_HTTPResponse_new(
        response_status_arg(status),
        headers,
        body
    );
}

LT_DEFINE_PRIMITIVE(
    response_new_simple,
    "Response class>>new:body:",
    "(self status body)",
    "Create an HTTP response without custom headers."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value status;
    LT_Value body;

    (void)tail_call_unwind_marker;
    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, status);
    LT_OBJECT_ARG(cursor, body);
    LT_ARG_END(cursor);
    (void)self;
    return (LT_Value)(uintptr_t)LT_HTTPResponse_new(
        response_status_arg(status),
        NULL,
        body
    );
}

#define RESPONSE_ACCESSOR(c_name, selector, expression, documentation)       \
    LT_DEFINE_PRIMITIVE(                                                     \
        c_name,                                                              \
        "Response>>" selector,                                          \
        "(self)",                                                           \
        documentation                                                       \
    ){                                                                       \
        LT_Value cursor = arguments;                                         \
        LT_HTTPResponse* self;                                               \
                                                                             \
        (void)tail_call_unwind_marker;                                       \
        LT_GENERIC_ARG(                                                      \
            cursor,                                                          \
            self,                                                            \
            LT_HTTPResponse*,                                               \
            LT_HTTPResponse_from_value                                      \
        );                                                                   \
        LT_ARG_END(cursor);                                                  \
        return (expression);                                                 \
    }

RESPONSE_ACCESSOR(
    response_status,
    "status",
    LT_Number_smallinteger_from_size(self->status, "Invalid HTTP status"),
    "Return the response status."
)
RESPONSE_ACCESSOR(
    response_headers,
    "headers",
    (LT_Value)(uintptr_t)self->headers,
    "Return the response headers."
)
RESPONSE_ACCESSOR(
    response_body,
    "body",
    self->body,
    "Return the response body."
)

LT_DEFINE_PRIMITIVE(
    streaming_response_set_status,
    "StreamingResponse>>setStatus:",
    "(self status)",
    "Set the response status before writing."
){
    LT_Value cursor = arguments;
    LT_Value status;
    LT_HTTPStreamingResponse* self;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(
        cursor,
        self,
        LT_HTTPStreamingResponse*,
        LT_HTTPStreamingResponse_from_value
    );
    LT_OBJECT_ARG(cursor, status);
    LT_ARG_END(cursor);
    LT_HTTPStreamingResponse_set_status(self, response_status_arg(status));
    return (LT_Value)(uintptr_t)self;
}

LT_DEFINE_PRIMITIVE(
    streaming_response_set_content_type,
    "StreamingResponse>>setContentType:",
    "(self contentType)",
    "Set the content type before writing."
){
    LT_Value cursor = arguments;
    LT_HTTPStreamingResponse* self;
    LT_String* content_type;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(
        cursor,
        self,
        LT_HTTPStreamingResponse*,
        LT_HTTPStreamingResponse_from_value
    );
    LT_GENERIC_ARG(cursor, content_type, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);
    LT_HTTPStreamingResponse_set_content_type(self, content_type);
    return (LT_Value)(uintptr_t)self;
}

LT_DEFINE_PRIMITIVE(
    streaming_response_header_at_put,
    "StreamingResponse>>headersAt:put:",
    "(self name value)",
    "Set a response header before writing."
){
    LT_Value cursor = arguments;
    LT_HTTPStreamingResponse* self;
    LT_String* name;
    LT_String* value;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(
        cursor,
        self,
        LT_HTTPStreamingResponse*,
        LT_HTTPStreamingResponse_from_value
    );
    LT_GENERIC_ARG(cursor, name, LT_String*, LT_String_from_value);
    LT_GENERIC_ARG(cursor, value, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);
    LT_HTTPStreamingResponse_header_at_put(self, name, value);
    return (LT_Value)(uintptr_t)self;
}

LT_DEFINE_PRIMITIVE(
    streaming_response_write,
    "StreamingResponse>>write:",
    "(self data)",
    "Write String or ByteVector data as one response chunk."
){
    LT_Value cursor = arguments;
    LT_Value data;
    LT_HTTPStreamingResponse* self;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(
        cursor,
        self,
        LT_HTTPStreamingResponse*,
        LT_HTTPStreamingResponse_from_value
    );
    LT_OBJECT_ARG(cursor, data);
    LT_ARG_END(cursor);
    LT_HTTPStreamingResponse_write(self, data);
    return (LT_Value)(uintptr_t)self;
}

LT_DEFINE_PRIMITIVE(
    streaming_response_send_data,
    "StreamingResponse>>sendData:",
    "(self data)",
    "Send one server-sent event containing data fields."
){
    LT_Value cursor = arguments;
    LT_Value data;
    LT_HTTPStreamingResponse* self;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(
        cursor,
        self,
        LT_HTTPStreamingResponse*,
        LT_HTTPStreamingResponse_from_value
    );
    LT_OBJECT_ARG(cursor, data);
    LT_ARG_END(cursor);
    LT_HTTPStreamingResponse_send_data(self, data);
    return (LT_Value)(uintptr_t)self;
}

LT_DEFINE_PRIMITIVE(
    streaming_response_send_event_data,
    "StreamingResponse>>sendEvent:Data:",
    "(self event data)",
    "Send a named server-sent event."
){
    LT_Value cursor = arguments;
    LT_Value data;
    LT_HTTPStreamingResponse* self;
    LT_String* event;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(
        cursor,
        self,
        LT_HTTPStreamingResponse*,
        LT_HTTPStreamingResponse_from_value
    );
    LT_GENERIC_ARG(cursor, event, LT_String*, LT_String_from_value);
    LT_OBJECT_ARG(cursor, data);
    LT_ARG_END(cursor);
    LT_HTTPStreamingResponse_send_event_data(self, event, data);
    return (LT_Value)(uintptr_t)self;
}

LT_DEFINE_PRIMITIVE(
    streaming_response_send_event_with_fields,
    "StreamingResponse>>sendEventWithFields:",
    "(self fields)",
    "Send a server-sent event from an alist or object understanding asAList."
){
    LT_Value cursor = arguments;
    LT_Value fields;
    LT_HTTPStreamingResponse* self;

    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(
        cursor,
        self,
        LT_HTTPStreamingResponse*,
        LT_HTTPStreamingResponse_from_value
    );
    LT_OBJECT_ARG(cursor, fields);
    LT_ARG_END(cursor);
    LT_HTTPStreamingResponse_send_event_with_fields(self, fields);
    return (LT_Value)(uintptr_t)self;
}

static LT_Method_Descriptor request_methods[] = {
    {"method", &req_method},
    {"target", &req_target},
    {"version", &req_version},
    {"headers", &req_headers},
    {"headerAt:", &req_header},
    {"body", &req_body},
    {"respond:body:", &req_respond},
    {"respond:headers:body:", &req_respond_headers},
    {"close", &req_close},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor server_methods[] = {
    {"accept", &server_accept},
    {"handle:", &server_handle},
    {"serve:", &server_serve},
    {"close", &server_close},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor server_class_methods[] = {
    {"newOn:port:", &server_new},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor response_methods[] = {
    {"status", &response_status},
    {"headers", &response_headers},
    {"body", &response_body},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor response_class_methods[] = {
    {"new:body:", &response_new_simple},
    {"new:headers:body:", &response_new},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor streaming_response_methods[] = {
    {"setStatus:", &streaming_response_set_status},
    {"setContentType:", &streaming_response_set_content_type},
    {"headersAt:put:", &streaming_response_header_at_put},
    {"write:", &streaming_response_write},
    {"sendData:", &streaming_response_send_data},
    {"sendEvent:Data:", &streaming_response_send_event_data},
    {"sendEventWithFields:", &streaming_response_send_event_with_fields},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_HTTPRequest) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .package = "ListTalk:HTTPD",
    .name = "Request",
    .documentation = "A parsed HTTP request.",
    .instance_size = sizeof(LT_HTTPRequest),
    .class_flags = LT_CLASS_FLAG_FINAL,
    .methods = request_methods,
};

LT_DEFINE_CLASS(LT_HTTPResponse) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .package = "ListTalk:HTTPD",
    .name = "Response",
    .documentation = "An HTTP handler result with explicit status and headers.",
    .instance_size = sizeof(LT_HTTPResponse),
    .class_flags = LT_CLASS_FLAG_FINAL | LT_CLASS_FLAG_IMMUTABLE,
    .methods = response_methods,
    .class_methods = response_class_methods,
};

LT_DEFINE_CLASS(LT_HTTPStreamingResponse) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .package = "ListTalk:HTTPD",
    .name = "StreamingResponse",
    .documentation = "A chunked response writer passed to a handler lambda.",
    .instance_size = sizeof(LT_HTTPStreamingResponse),
    .class_flags = LT_CLASS_FLAG_FINAL,
    .methods = streaming_response_methods,
};

LT_DEFINE_CLASS(LT_HTTPServer) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .package = "ListTalk:HTTPD",
    .name = "Server",
    .documentation = "A synchronous HTTP server.",
    .instance_size = sizeof(LT_HTTPServer),
    .class_flags = LT_CLASS_FLAG_FINAL,
    .methods = server_methods,
    .class_methods = server_class_methods,
};
