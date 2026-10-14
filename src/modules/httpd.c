/* SPDX-License-Identifier: MIT */
#include <ListTalk/ListTalk.h>
#include <ListTalk/networking/HTTP.h>

void ListTalk_httpd_load(LT_Environment* environment){
    LT_Package* package = LT_Package_new("ListTalk:HTTPD");

#define BIND_CLASS(name, class_name)                                        \
    LT_Environment_bind(                                                    \
        environment,                                                        \
        LT_Symbol_new_in(package, name),                                    \
        LT_STATIC_CLASS(class_name),                                        \
        LT_ENV_BINDING_FLAG_CONSTANT                                       \
    )
    BIND_CLASS("Server", LT_HTTPServer);
    BIND_CLASS("Request", LT_HTTPRequest);
    BIND_CLASS("Response", LT_HTTPResponse);
    BIND_CLASS("StreamingResponse", LT_HTTPStreamingResponse);
#undef BIND_CLASS
    LT_loader_provide(environment, "httpd");
}
