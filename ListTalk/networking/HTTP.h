/* SPDX-License-Identifier: MIT */
#ifndef H__ListTalk__networking__HTTP__
#define H__ListTalk__networking__HTTP__

#include <ListTalk/classes/Dictionary.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/networking/Socket.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_HTTPRequest);
LT_DECLARE_CLASS(LT_HTTPResponse);
LT_DECLARE_CLASS(LT_HTTPStreamingResponse);
LT_DECLARE_CLASS(LT_HTTPServer);

/* Header names are stored in lower case, making lookup case insensitive. */
LT_String* LT_HTTP_header_name(const char* bytes, size_t length);
int LT_HTTP_parse_header(LT_Dictionary* headers,
                         const uint8_t* bytes,
                         size_t length);

LT_HTTPRequest* LT_HTTPRequest_read(LT_TCPSocket* socket);
LT_String* LT_HTTPRequest_method(LT_HTTPRequest* request);
LT_String* LT_HTTPRequest_target(LT_HTTPRequest* request);
LT_String* LT_HTTPRequest_version(LT_HTTPRequest* request);
LT_ImmutableDictionary* LT_HTTPRequest_headers(LT_HTTPRequest* request);
LT_Value LT_HTTPRequest_header(LT_HTTPRequest* request, LT_String* name);
LT_ByteVector* LT_HTTPRequest_read_body(LT_HTTPRequest* request);
void LT_HTTPRequest_respond(LT_HTTPRequest* request,
                            unsigned status,
                            LT_Dictionary* headers,
                            const void* body,
                            size_t body_length);
void LT_HTTPRequest_respond_with(LT_HTTPRequest* request, LT_Value result);
void LT_HTTPRequest_close(LT_HTTPRequest* request);

LT_HTTPResponse* LT_HTTPResponse_new(unsigned status,
                                     LT_Dictionary* headers,
                                     LT_Value body);
unsigned LT_HTTPResponse_status(LT_HTTPResponse* response);
LT_ImmutableDictionary* LT_HTTPResponse_headers(LT_HTTPResponse* response);
LT_Value LT_HTTPResponse_body(LT_HTTPResponse* response);

void LT_HTTPStreamingResponse_set_status(LT_HTTPStreamingResponse* response,
                                         unsigned status);
void LT_HTTPStreamingResponse_set_content_type(
    LT_HTTPStreamingResponse* response,
    LT_String* content_type
);
void LT_HTTPStreamingResponse_header_at_put(
    LT_HTTPStreamingResponse* response,
    LT_String* name,
    LT_String* value
);
void LT_HTTPStreamingResponse_write(LT_HTTPStreamingResponse* response,
                                    LT_Value data);
void LT_HTTPStreamingResponse_send_data(LT_HTTPStreamingResponse* response,
                                        LT_Value data);
void LT_HTTPStreamingResponse_send_event_data(
    LT_HTTPStreamingResponse* response,
    LT_String* event,
    LT_Value data
);
void LT_HTTPStreamingResponse_send_event_with_fields(
    LT_HTTPStreamingResponse* response,
    LT_Value fields
);

LT_HTTPServer* LT_HTTPServer_new(const char* host, uint16_t port, int backlog);
LT_HTTPRequest* LT_HTTPServer_accept(LT_HTTPServer* server);
LT_Value LT_HTTPServer_handle(LT_HTTPServer* server, LT_Value handler);
void LT_HTTPServer_close(LT_HTTPServer* server);

LT__END_DECLS
#endif
