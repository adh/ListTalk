/* SPDX-License-Identifier: MIT */
#include <ListTalk/ListTalk.h>
#include <ListTalk/networking/Socket.h>

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

    LT_INIT();
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
