/* SPDX-License-Identifier: MIT */
#include <ListTalk/networking/Socket.h>
#include <ListTalk/classes/Class.h>
#include <ListTalk/classes/Object.h>

LT_DEFINE_CLASS(LT_Socket) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .package = "ListTalk:Socket",
    .name = "Socket",
    .documentation = "Abstract socket protocol root.",
    .instance_size = 0,
    .class_flags = LT_CLASS_FLAG_ABSTRACT,
};

LT_DEFINE_CLASS(LT_DatagramSocket) {
    .superclass = &LT_Socket_class,
    .metaclass_superclass = &LT_Class_class,
    .package = "ListTalk:Socket",
    .name = "DatagramSocket",
    .documentation = "Abstract message-oriented socket.",
    .instance_size = 0,
    .class_flags = LT_CLASS_FLAG_ABSTRACT,
};

LT_DEFINE_CLASS(LT_StreamSocket) {
    .superclass = &LT_Socket_class,
    .metaclass_superclass = &LT_Class_class,
    .package = "ListTalk:Socket",
    .name = "StreamSocket",
    .documentation = "Abstract connected byte-stream socket.",
    .instance_size = 0,
    .class_flags = LT_CLASS_FLAG_ABSTRACT,
};

LT_DEFINE_CLASS(LT_ServerSocket) {
    .superclass = &LT_Socket_class,
    .metaclass_superclass = &LT_Class_class,
    .package = "ListTalk:Socket",
    .name = "ServerSocket",
    .documentation = "Abstract socket that accepts connections.",
    .instance_size = 0,
    .class_flags = LT_CLASS_FLAG_ABSTRACT,
};
