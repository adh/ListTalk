/*
 * SPDX-License-Identifier: MIT
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Boolean.h>
#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/Dictionary.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Package.h>
#include <ListTalk/classes/String.h>

#include <curl/curl.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>

LT_DECLARE_CLASS(LT_CURL);
LT_DECLARE_CLASS(LT_CURLResponse);

struct LT_CURL_s {
    LT_Object base;
    LT_String* url;
    LT_String* username;
    LT_String* password;
    LT_String* method;
    LT_Dictionary* headers;
    LT_Value post_data;
    LT_Value upload_data;
    int follow_redirects;
};

struct LT_CURLResponse_s {
    LT_Object base;
    LT_ByteVector* content;
    LT_Value status;
    LT_ImmutableDictionary* headers;
};

struct CURLTransfer {
    LT_StringBuilder* content;
    LT_ImmutableDictionary* headers;
    const uint8_t* upload_data;
    size_t upload_length;
    size_t upload_offset;
};

static const uint8_t* curl_data_bytes(LT_Value value, size_t* length_out){
    if (LT_ByteVector_p(value)){
        LT_ByteVector* bytes = LT_ByteVector_from_value(value);

        *length_out = LT_ByteVector_length(bytes);
        return LT_ByteVector_bytes(bytes);
    }
    if (LT_String_p(value)){
        LT_String* string = LT_String_from_value(value);

        *length_out = LT_String_byte_length(string);
        return (const uint8_t*)LT_String_value_cstr(string);
    }
    LT_error("CURL data must be a ByteVector or String");
    return NULL;
}

static LT_String* header_name(LT_String* name){
    const char* input = LT_String_value_cstr(name);
    size_t length = LT_String_byte_length(name);
    char* normalized = GC_MALLOC_ATOMIC(length == 0 ? 1 : length);
    size_t i;

    for (i = 0; i < length; i++){
        normalized[i] = (char)tolower((unsigned char)input[i]);
    }
    return LT_String_new(normalized, length);
}

static void curl_check(CURLcode result, const char* operation){
    if (result != CURLE_OK){
        LT_error(LT_sprintf("curl %s failed: %s", operation, curl_easy_strerror(result)));
    }
}

static void curl_configure(CURLcode result,
                           CURL* handle,
                           struct curl_slist* headers,
                           const char* operation){
    if (result != CURLE_OK){
        curl_slist_free_all(headers);
        curl_easy_cleanup(handle);
        curl_check(result, operation);
    }
}

static size_t response_body_callback(char* data, size_t size, size_t count, void* user_data){
    struct CURLTransfer* transfer = user_data;
    size_t length = size * count;

    if (size != 0 && length / size != count){
        return 0;
    }
    LT_StringBuilder_append_bytes(transfer->content, data, length);
    return length;
}

static size_t response_header_callback(char* data, size_t size, size_t count, void* user_data){
    struct CURLTransfer* transfer = user_data;
    size_t length = size * count;
    char* colon;
    char* end;
    char* value;

    if (size != 0 && length / size != count){
        return 0;
    }
    if (length >= 5 && memcmp(data, "HTTP/", 5) == 0){
        transfer->headers = LT_ImmutableDictionary_new();
        return length;
    }
    colon = memchr(data, ':', length);
    if (colon == NULL){
        return length;
    }
    end = data + length;
    value = colon + 1;
    while (value < end && (*value == ' ' || *value == '\t')){
        value++;
    }
    while (end > value && (end[-1] == '\r' || end[-1] == '\n'
                           || end[-1] == ' ' || end[-1] == '\t')){
        end--;
    }
    LT_Dictionary_atPut(
        (LT_Dictionary*)transfer->headers,
        (LT_Value)(uintptr_t)header_name(
            LT_String_new(data, (size_t)(colon - data))
        ),
        (LT_Value)(uintptr_t)LT_String_new(value, (size_t)(end - value))
    );
    return length;
}

static size_t request_body_callback(char* data, size_t size, size_t count, void* user_data){
    struct CURLTransfer* transfer = user_data;
    size_t capacity = size * count;
    size_t remaining;
    size_t length;

    if (size != 0 && capacity / size != count){
        return CURL_READFUNC_ABORT;
    }
    remaining = transfer->upload_length - transfer->upload_offset;
    length = remaining < capacity ? remaining : capacity;
    if (length > 0){
        memcpy(data, transfer->upload_data + transfer->upload_offset, length);
        transfer->upload_offset += length;
    }
    return length;
}

static LT_CURL* curl_new(LT_String* url){
    LT_CURL* request = LT_Class_ALLOC(LT_CURL);

    request->url = url;
    request->username = NULL;
    request->password = NULL;
    request->method = NULL;
    request->headers = LT_Dictionary_new();
    request->post_data = LT_NIL;
    request->upload_data = LT_NIL;
    request->follow_redirects = 1;
    return request;
}

LT_DEFINE_PRIMITIVE(
    curl_method_headers_at_put,
    "CURL>>headersAt:put:",
    "(self name value)",
    "Set a request header. Header names are case-insensitive."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_CURL* request;
    LT_String* name;
    LT_String* value;

    (void)tail_call_unwind_marker;
    LT_OBJECT_ARG(cursor, self);
    request = LT_CURL_from_value(self);
    LT_GENERIC_ARG(cursor, name, LT_String*, LT_String_from_value);
    LT_GENERIC_ARG(cursor, value, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);

    if (strpbrk(LT_String_value_cstr(name), ":\r\n") != NULL
        || strpbrk(LT_String_value_cstr(value), "\r\n") != NULL){
        LT_error("Invalid header name/value (must not contain ':' or newlines)");
    }

    LT_Dictionary_atPut(
        request->headers,
        (LT_Value)(uintptr_t)header_name(name),
        (LT_Value)(uintptr_t)value
    );
    return self;
}

#define CURL_STRING_SETTER(c_name, selector, field, description)            \
    LT_DEFINE_PRIMITIVE(c_name, "CURL>>" selector, "(self value)", description){ \
        LT_Value cursor = arguments;                                        \
        LT_Value self;                                                      \
        LT_CURL* request;                                                   \
        LT_String* value;                                                   \
        (void)tail_call_unwind_marker;                                      \
        LT_OBJECT_ARG(cursor, self);                                        \
        request = LT_CURL_from_value(self);                                 \
        LT_GENERIC_ARG(cursor, value, LT_String*, LT_String_from_value);     \
        LT_ARG_END(cursor);                                                 \
        request->field = value;                                             \
        return self;                                                        \
    }

CURL_STRING_SETTER(curl_method_username, "username:", username,
                   "Set the username used for authentication.")
CURL_STRING_SETTER(curl_method_password, "password:", password,
                   "Set the password used for authentication.")
CURL_STRING_SETTER(curl_method_method, "method:", method,
                   "Set the request method.")

LT_DEFINE_PRIMITIVE(
    curl_method_post_data,
    "CURL>>postData:",
    "(self data)",
    "Set String or ByteVector data to send in a POST request."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value data;
    LT_CURL* request;
    size_t ignored;

    (void)tail_call_unwind_marker;
    LT_OBJECT_ARG(cursor, self);
    request = LT_CURL_from_value(self);
    LT_OBJECT_ARG(cursor, data);
    LT_ARG_END(cursor);
    (void)curl_data_bytes(data, &ignored);
    request->post_data = data;
    request->upload_data = LT_NIL;
    return self;
}

LT_DEFINE_PRIMITIVE(
    curl_method_upload_data,
    "CURL>>uploadData:",
    "(self data)",
    "Set String or ByteVector data to upload."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value data;
    LT_CURL* request;
    size_t ignored;

    (void)tail_call_unwind_marker;
    LT_OBJECT_ARG(cursor, self);
    request = LT_CURL_from_value(self);
    LT_OBJECT_ARG(cursor, data);
    LT_ARG_END(cursor);
    (void)curl_data_bytes(data, &ignored);
    request->upload_data = data;
    request->post_data = LT_NIL;
    return self;
}

static struct curl_slist* curl_request_headers(LT_CURL* request){
    LT_Value entries = LT_Dictionary_asAList(request->headers);
    struct curl_slist* headers = NULL;

    while (entries != LT_NIL){
        LT_Value entry = LT_car(entries);
        LT_String* name = LT_String_from_value(LT_car(entry));
        LT_String* value = LT_String_from_value(LT_cdr(entry));
        char* line = LT_sprintf("%s: %s", LT_String_value_cstr(name),
                                LT_String_value_cstr(value));
        struct curl_slist* appended = curl_slist_append(headers, line);

        if (appended == NULL){
            curl_slist_free_all(headers);
            LT_error("Could not allocate CURL request headers");
        }
        headers = appended;
        entries = LT_cdr(entries);
    }
    return headers;
}

LT_DEFINE_PRIMITIVE(
    curl_class_method_new,
    "CURL class>>new:",
    "(self url)",
    "Create a CURL request for URL. Redirects are followed by default."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* url;

    (void)tail_call_unwind_marker;
    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, url, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);
    if (self != LT_STATIC_CLASS(LT_CURL)){
        LT_error("new: class method is only supported on CURL");
    }
    return (LT_Value)(uintptr_t)curl_new(url);
}

LT_DEFINE_PRIMITIVE(
    curl_method_follow_redirects,
    "CURL>>followRedirects:",
    "(self follow-redirects)",
    "Set whether redirects are followed."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value follow_redirects;
    LT_CURL* request;

    (void)tail_call_unwind_marker;
    LT_OBJECT_ARG(cursor, self);
    request = LT_CURL_from_value(self);
    LT_OBJECT_ARG(cursor, follow_redirects);
    LT_ARG_END(cursor);
    if (follow_redirects != LT_TRUE && follow_redirects != LT_FALSE){
        LT_type_error(follow_redirects, &LT_Boolean_class);
    }
    request->follow_redirects = follow_redirects == LT_TRUE;
    return self;
}

LT_DEFINE_PRIMITIVE(
    curl_method_perform,
    "CURL>>perform!",
    "(self)",
    "Perform the request and return its Response."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_CURL* request;
    LT_CURLResponse* response;
    struct CURLTransfer transfer;
    struct curl_slist* request_headers;
    CURL* handle;
    CURLcode result;
    long status;

    (void)tail_call_unwind_marker;
    LT_OBJECT_ARG(cursor, self);
    request = LT_CURL_from_value(self);
    LT_ARG_END(cursor);

    transfer.content = LT_StringBuilder_new();
    transfer.headers = LT_ImmutableDictionary_new();
    transfer.upload_data = NULL;
    transfer.upload_length = 0;
    transfer.upload_offset = 0;
    request_headers = curl_request_headers(request);
    handle = curl_easy_init();
    if (handle == NULL){
        curl_slist_free_all(request_headers);
        LT_error("Could not create CURL handle");
    }
    curl_configure(curl_easy_setopt(handle, CURLOPT_URL,
                                    LT_String_value_cstr(request->url)),
                   handle, request_headers, "setting URL");
    curl_configure(curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION,
                                    request->follow_redirects ? 1L : 0L),
                   handle, request_headers, "setting redirects");
    if (request->username != NULL){
        curl_configure(curl_easy_setopt(handle, CURLOPT_USERNAME,
                                        LT_String_value_cstr(request->username)),
                       handle, request_headers, "setting username");
    }
    if (request->password != NULL){
        curl_configure(curl_easy_setopt(handle, CURLOPT_PASSWORD,
                                        LT_String_value_cstr(request->password)),
                       handle, request_headers, "setting password");
    }
    if (request->method != NULL){
        curl_configure(curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST,
                                        LT_String_value_cstr(request->method)),
                       handle, request_headers, "setting method");
    }
    if (request->post_data != LT_NIL){
        size_t length;
        const uint8_t* data = curl_data_bytes(request->post_data, &length);

        curl_configure(curl_easy_setopt(handle, CURLOPT_POST, 1L),
                       handle, request_headers, "enabling POST");
        curl_configure(curl_easy_setopt(handle, CURLOPT_POSTFIELDS, data),
                       handle, request_headers, "setting POST data");
        curl_configure(curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE_LARGE,
                                        (curl_off_t)length),
                       handle, request_headers, "setting POST data length");
    } else if (request->upload_data != LT_NIL){
        transfer.upload_data = curl_data_bytes(request->upload_data,
                                               &transfer.upload_length);
        curl_configure(curl_easy_setopt(handle, CURLOPT_UPLOAD, 1L),
                       handle, request_headers, "enabling upload");
        curl_configure(curl_easy_setopt(handle, CURLOPT_READFUNCTION,
                                        request_body_callback),
                       handle, request_headers, "setting upload callback");
        curl_configure(curl_easy_setopt(handle, CURLOPT_READDATA, &transfer),
                       handle, request_headers, "setting upload callback data");
        curl_configure(curl_easy_setopt(handle, CURLOPT_INFILESIZE_LARGE,
                                        (curl_off_t)transfer.upload_length),
                       handle, request_headers, "setting upload data length");
    }
    curl_configure(curl_easy_setopt(handle, CURLOPT_HTTPHEADER, request_headers),
                   handle, request_headers, "setting request headers");
    curl_configure(curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION,
                                    response_body_callback),
                   handle, request_headers, "setting body callback");
    curl_configure(curl_easy_setopt(handle, CURLOPT_WRITEDATA, &transfer),
                   handle, request_headers, "setting body callback data");
    curl_configure(curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION,
                                    response_header_callback),
                   handle, request_headers, "setting header callback");
    curl_configure(curl_easy_setopt(handle, CURLOPT_HEADERDATA, &transfer),
                   handle, request_headers, "setting header callback data");
    result = curl_easy_perform(handle);
    curl_slist_free_all(request_headers);
    if (result != CURLE_OK){
        curl_easy_cleanup(handle);
        curl_check(result, "request");
    }
    result = curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(handle);
    curl_check(result, "reading response status");

    response = LT_Class_ALLOC(LT_CURLResponse);
    response->content = LT_ByteVector_new(
        (uint8_t*)LT_StringBuilder_value(transfer.content),
        LT_StringBuilder_length(transfer.content)
    );
    response->status = LT_Number_smallinteger_from_long(status,
                                                        "HTTP status out of range");
    response->headers = transfer.headers;
    return (LT_Value)(uintptr_t)response;
}

#define RESPONSE_ACCESSOR(c_name, selector, field, description)             \
    LT_DEFINE_PRIMITIVE(c_name, "Response>>" selector, "(self)", description){ \
        LT_Value cursor = arguments;                                        \
        LT_Value self;                                                      \
        LT_CURLResponse* response;                                          \
        (void)tail_call_unwind_marker;                                      \
        LT_OBJECT_ARG(cursor, self);                                        \
        response = LT_CURLResponse_from_value(self);                        \
        LT_ARG_END(cursor);                                                 \
        return (LT_Value)(uintptr_t)response->field;                         \
    }

RESPONSE_ACCESSOR(response_method_content, "content", content,
                  "Return the response body as a ByteVector.")
RESPONSE_ACCESSOR(response_method_status, "status", status,
                  "Return the response status code, or zero when unavailable.")
RESPONSE_ACCESSOR(response_method_headers, "headers", headers,
                  "Return the response headers as a dictionary.")

LT_DEFINE_PRIMITIVE(
    response_method_header_at,
    "Response>>headerAt:",
    "(self name)",
    "Return a response header value, or nil when it is absent."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_CURLResponse* response;
    LT_String* name;
    LT_Value value;

    (void)tail_call_unwind_marker;
    LT_OBJECT_ARG(cursor, self);
    response = LT_CURLResponse_from_value(self);
    LT_GENERIC_ARG(cursor, name, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);
    if (!LT_Dictionary_at((LT_Dictionary*)response->headers,
                          (LT_Value)(uintptr_t)header_name(name), &value)){
        return LT_NIL;
    }
    return value;
}

static LT_Method_Descriptor CURL_methods[] = {
    {"followRedirects:", &curl_method_follow_redirects},
    {"headersAt:put:", &curl_method_headers_at_put},
    {"username:", &curl_method_username},
    {"password:", &curl_method_password},
    {"method:", &curl_method_method},
    {"postData:", &curl_method_post_data},
    {"uploadData:", &curl_method_upload_data},
    {"perform!", &curl_method_perform},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor CURL_class_methods[] = {
    {"new:", &curl_class_method_new},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor Response_methods[] = {
    {"content", &response_method_content},
    {"status", &response_method_status},
    {"headers", &response_method_headers},
    {"headerAt:", &response_method_header_at},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_CURL) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .package = "ListTalk:curl",
    .name = "CURL",
    .documentation = "A libcurl easy request.",
    .instance_size = sizeof(LT_CURL),
    .class_flags = LT_CLASS_FLAG_FINAL,
    .methods = CURL_methods,
    .class_methods = CURL_class_methods,
};

LT_DEFINE_CLASS(LT_CURLResponse) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .package = "ListTalk:curl",
    .name = "Response",
    .documentation = "The result of a CURL request.",
    .instance_size = sizeof(LT_CURLResponse),
    .class_flags = LT_CLASS_FLAG_FINAL | LT_CLASS_FLAG_IMMUTABLE,
    .methods = Response_methods,
};

void ListTalk_curl_load(LT_Environment* environment){
    LT_Package* package = LT_Package_new("ListTalk:curl");

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK){
        LT_error("Could not initialize libcurl");
    }
    LT_Environment_bind(environment, LT_Symbol_new_in(package, "CURL"),
                        LT_STATIC_CLASS(LT_CURL), LT_ENV_BINDING_FLAG_CONSTANT);
    LT_Environment_bind(environment, LT_Symbol_new_in(package, "Response"),
                        LT_STATIC_CLASS(LT_CURLResponse), LT_ENV_BINDING_FLAG_CONSTANT);
    LT_loader_provide(environment, "curl");
}
