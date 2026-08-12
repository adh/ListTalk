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
#include <stdint.h>
#include <string.h>

LT_DECLARE_CLASS(LT_CURL);
LT_DECLARE_CLASS(LT_CURLResponse);

struct LT_CURL_s {
    LT_Object base;
    CURL* handle;
    LT_String* url;
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
};

static void curl_check(CURLcode result, const char* operation){
    if (result != CURLE_OK){
        LT_error(LT_sprintf("curl %s failed: %s", operation, curl_easy_strerror(result)));
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
        (LT_Value)(uintptr_t)LT_String_new(data, (size_t)(colon - data)),
        (LT_Value)(uintptr_t)LT_String_new(value, (size_t)(end - value))
    );
    return length;
}

static void curl_finalizer(void* object, void* data){
    LT_CURL* request = object;

    (void)data;
    if (request->handle != NULL){
        curl_easy_cleanup(request->handle);
        request->handle = NULL;
    }
}

static LT_CURL* curl_new(LT_String* url){
    LT_CURL* request = LT_Class_ALLOC(LT_CURL);

    request->handle = curl_easy_init();
    if (request->handle == NULL){
        LT_error("Could not create CURL handle");
    }
    request->url = url;
    request->follow_redirects = 1;
    GC_register_finalizer(request, curl_finalizer, NULL, NULL, NULL);
    return request;
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
    long status;

    (void)tail_call_unwind_marker;
    LT_OBJECT_ARG(cursor, self);
    request = LT_CURL_from_value(self);
    LT_ARG_END(cursor);

    transfer.content = LT_StringBuilder_new();
    transfer.headers = LT_ImmutableDictionary_new();
    curl_easy_reset(request->handle);
    curl_check(curl_easy_setopt(request->handle, CURLOPT_URL,
                               LT_String_value_cstr(request->url)), "setting URL");
    curl_check(curl_easy_setopt(request->handle, CURLOPT_FOLLOWLOCATION,
                               request->follow_redirects ? 1L : 0L), "setting redirects");
    curl_check(curl_easy_setopt(request->handle, CURLOPT_WRITEFUNCTION,
                               response_body_callback), "setting body callback");
    curl_check(curl_easy_setopt(request->handle, CURLOPT_WRITEDATA,
                               &transfer), "setting body callback data");
    curl_check(curl_easy_setopt(request->handle, CURLOPT_HEADERFUNCTION,
                               response_header_callback), "setting header callback");
    curl_check(curl_easy_setopt(request->handle, CURLOPT_HEADERDATA,
                               &transfer), "setting header callback data");
    curl_check(curl_easy_perform(request->handle), "request");
    curl_check(curl_easy_getinfo(request->handle, CURLINFO_RESPONSE_CODE, &status),
               "reading response status");

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

static LT_Method_Descriptor CURL_methods[] = {
    {"followRedirects:", &curl_method_follow_redirects},
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
