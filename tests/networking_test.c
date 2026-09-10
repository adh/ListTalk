/* SPDX-License-Identifier: MIT */
#include <ListTalk/ListTalk.h>
#include <ListTalk/networking/Socket.h>

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

static int failures;

static void check(int condition, const char* message){
    if (!condition){
        fprintf(stderr, "FAIL: %s\n", message);
        failures++;
    }
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
    LT_TCPSocket *client, *peer;
    LT_UDPSocket *receiver, *sender;
    LT_ByteVector *message, *received;
    char reply[4];

    LT_INIT();
    server = LT_TCPServerSocket_listen("127.0.0.1", 0, 4);
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
    LT_IPSocket_close((LT_IPSocket*)peer);
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
    return failures ? 1 : 0;
}
