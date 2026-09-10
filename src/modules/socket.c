/* SPDX-License-Identifier: MIT */
#include <ListTalk/ListTalk.h>
#include <ListTalk/networking/Socket.h>

void ListTalk_socket_load(LT_Environment* environment){
    LT_Package* package = LT_Package_new("ListTalk:Socket");
#define BIND_CLASS(name) LT_Environment_bind(environment, LT_Symbol_new_in(package, #name), LT_STATIC_CLASS(LT_##name), LT_ENV_BINDING_FLAG_CONSTANT)
    BIND_CLASS(Socket);
    BIND_CLASS(DatagramSocket);
    BIND_CLASS(StreamSocket);
    BIND_CLASS(ServerSocket);
    BIND_CLASS(IPSocket);
    BIND_CLASS(UDPSocket);
    BIND_CLASS(TCPSocket);
    BIND_CLASS(TCPServerSocket);
#undef BIND_CLASS
    LT_loader_provide(environment, "socket");
}
