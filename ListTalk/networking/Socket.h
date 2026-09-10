/* SPDX-License-Identifier: MIT */
#ifndef H__ListTalk__networking__Socket__
#define H__ListTalk__networking__Socket__

#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Socket);
LT_DECLARE_CLASS(LT_DatagramSocket);
LT_DECLARE_CLASS(LT_StreamSocket);
LT_DECLARE_CLASS(LT_ServerSocket);
LT_DECLARE_CLASS(LT_IPSocket);
LT_DECLARE_CLASS(LT_UDPSocket);
LT_DECLARE_CLASS(LT_TCPSocket);
LT_DECLARE_CLASS(LT_TCPServerSocket);
LT_DECLARE_CLASS(LT_UnixSocket);
LT_DECLARE_CLASS(LT_UnixDatagramSocket);
LT_DECLARE_CLASS(LT_UnixStreamSocket);
LT_DECLARE_CLASS(LT_UnixServerSocket);

int LT_IPSocket_closed(LT_IPSocket* socket);
int LT_IPSocket_descriptor(LT_IPSocket* socket);
void LT_IPSocket_close(LT_IPSocket* socket);

LT_UDPSocket* LT_UDPSocket_bind(const char* host, uint16_t port);
LT_UDPSocket* LT_UDPSocket_connect(const char* host, uint16_t port);
size_t LT_UDPSocket_send(LT_UDPSocket* socket, LT_ByteVector* bytes);
LT_ByteVector* LT_UDPSocket_receive(LT_UDPSocket* socket,
                                    size_t maximum_length);

LT_TCPSocket* LT_TCPSocket_connect(const char* host, uint16_t port);
size_t LT_TCPSocket_read(LT_TCPSocket* socket,
                         void* buffer, size_t length);
void LT_TCPSocket_write(LT_TCPSocket* socket,
                        const void* buffer, size_t length);
void LT_TCPSocket_shutdown_write(LT_TCPSocket* socket);

LT_TCPServerSocket* LT_TCPServerSocket_new(const char* host,
                                           uint16_t port,
                                           int backlog);
LT_TCPSocket* LT_TCPServerSocket_accept(LT_TCPServerSocket* socket);

int LT_UnixSocket_closed(LT_UnixSocket* socket);
int LT_UnixSocket_descriptor(LT_UnixSocket* socket);
void LT_UnixSocket_close(LT_UnixSocket* socket);

LT_UnixDatagramSocket* LT_UnixDatagramSocket_bind(const char* path);
LT_UnixDatagramSocket* LT_UnixDatagramSocket_connect(const char* path);
size_t LT_UnixDatagramSocket_send(LT_UnixDatagramSocket* socket,
                                  LT_ByteVector* bytes);
LT_ByteVector* LT_UnixDatagramSocket_receive(
    LT_UnixDatagramSocket* socket,
    size_t maximum_length
);

LT_UnixStreamSocket* LT_UnixStreamSocket_connect(const char* path);
void LT_UnixStreamSocket_pair(LT_UnixStreamSocket** first,
                              LT_UnixStreamSocket** second);
size_t LT_UnixStreamSocket_read(LT_UnixStreamSocket* socket,
                                void* buffer,
                                size_t length);
void LT_UnixStreamSocket_write(LT_UnixStreamSocket* socket,
                               const void* buffer,
                               size_t length);
void LT_UnixStreamSocket_shutdown_write(LT_UnixStreamSocket* socket);

LT_UnixServerSocket* LT_UnixServerSocket_new(const char* path, int backlog);
LT_UnixStreamSocket* LT_UnixServerSocket_accept(
    LT_UnixServerSocket* socket
);

LT__END_DECLS
#endif
