/*
 * SPDX-License-Identifier: MIT
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/Dictionary.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/vm/throw_catch.h>

#include <expat.h>
#include <stdint.h>

LT_DECLARE_CLASS(LT_XMLEventHandler);

struct LT_XMLEventHandler_s {
    LT_Object base;
};

struct XMLParseContext {
    LT_Value handler;
};

static LT_Value xml_string(const XML_Char* string){
    return (LT_Value)(uintptr_t)LT_String_new_cstr((char*)string);
}

static LT_Value xml_string_with_length(const XML_Char* string, int length){
    return (LT_Value)(uintptr_t)LT_String_new((char*)string, (size_t)length);
}

static LT_Value xml_attributes(const XML_Char** attributes){
    LT_ImmutableDictionary* dictionary = LT_ImmutableDictionary_new();

    while (*attributes != NULL){
        LT_Dictionary_atPut(
            (LT_Dictionary*)dictionary,
            xml_string(attributes[0]),
            xml_string(attributes[1])
        );
        attributes += 2;
    }
    return (LT_Value)(uintptr_t)dictionary;
}

static void XMLCALL xml_start_element(void* user_data,
                                      const XML_Char* name,
                                      const XML_Char** attributes){
    struct XMLParseContext* context = user_data;

    (void)LT_SEND(
        context->handler,
        "startElement:attributes:",
        xml_string(name),
        xml_attributes(attributes)
    );
}

static void XMLCALL xml_end_element(void* user_data, const XML_Char* name){
    struct XMLParseContext* context = user_data;

    (void)LT_SEND(context->handler, "endElement:", xml_string(name));
}

static void XMLCALL xml_character_data(void* user_data,
                                       const XML_Char* data,
                                       int length){
    struct XMLParseContext* context = user_data;

    (void)LT_SEND(
        context->handler,
        "characters:",
        xml_string_with_length(data, length)
    );
}

static void XMLCALL xml_processing_instruction(void* user_data,
                                               const XML_Char* target,
                                               const XML_Char* data){
    struct XMLParseContext* context = user_data;

    (void)LT_SEND(
        context->handler,
        "processingInstruction:data:",
        xml_string(target),
        xml_string(data)
    );
}

static void XMLCALL xml_comment(void* user_data, const XML_Char* data){
    struct XMLParseContext* context = user_data;

    (void)LT_SEND(context->handler, "comment:", xml_string(data));
}

static void XMLCALL xml_start_cdata(void* user_data){
    struct XMLParseContext* context = user_data;

    (void)LT_SEND(context->handler, "startCDATA");
}

static void XMLCALL xml_end_cdata(void* user_data){
    struct XMLParseContext* context = user_data;

    (void)LT_SEND(context->handler, "endCDATA");
}

#define XML_NOOP_METHOD_0(c_name, primitive_name, selector, description) \
    LT_DEFINE_PRIMITIVE(                                                 \
        c_name,                                                         \
        primitive_name,                                                 \
        "(self)",                                                      \
        description                                                     \
    ){                                                                  \
        LT_Value cursor = arguments;                                    \
        LT_Value self;                                                  \
                                                                        \
        (void)tail_call_unwind_marker;                                  \
        (void)invocation_context_kind;                                  \
        (void)invocation_context_data;                                  \
        (void)selector;                                                 \
        LT_OBJECT_ARG(cursor, self);                                    \
        LT_ARG_END(cursor);                                             \
        return self;                                                    \
    }

#define XML_NOOP_METHOD_1(c_name, primitive_name, arg1, description) \
    LT_DEFINE_PRIMITIVE(                                             \
        c_name,                                                     \
        primitive_name,                                             \
        "(self " #arg1 ")",                                      \
        description                                                 \
    ){                                                              \
        LT_Value cursor = arguments;                                \
        LT_Value self;                                              \
        LT_Value arg1;                                              \
                                                                    \
        (void)tail_call_unwind_marker;                              \
        (void)invocation_context_kind;                              \
        (void)invocation_context_data;                              \
        LT_OBJECT_ARG(cursor, self);                                \
        LT_OBJECT_ARG(cursor, arg1);                                \
        LT_ARG_END(cursor);                                         \
        (void)arg1;                                                 \
        return self;                                                \
    }

#define XML_NOOP_METHOD_2(c_name, primitive_name, arg1, arg2, description) \
    LT_DEFINE_PRIMITIVE(                                                    \
        c_name,                                                            \
        primitive_name,                                                    \
        "(self " #arg1 " " #arg2 ")",                                  \
        description                                                        \
    ){                                                                     \
        LT_Value cursor = arguments;                                       \
        LT_Value self;                                                     \
        LT_Value arg1;                                                     \
        LT_Value arg2;                                                     \
                                                                           \
        (void)tail_call_unwind_marker;                                     \
        (void)invocation_context_kind;                                     \
        (void)invocation_context_data;                                     \
        LT_OBJECT_ARG(cursor, self);                                       \
        LT_OBJECT_ARG(cursor, arg1);                                       \
        LT_OBJECT_ARG(cursor, arg2);                                       \
        LT_ARG_END(cursor);                                                \
        (void)arg1;                                                        \
        (void)arg2;                                                        \
        return self;                                                       \
    }

XML_NOOP_METHOD_0(
    xml_handler_method_start_document,
    "XMLEventHandler>>startDocument",
    "startDocument",
    "Handle the start of an XML document."
)

XML_NOOP_METHOD_0(
    xml_handler_method_end_document,
    "XMLEventHandler>>endDocument",
    "endDocument",
    "Handle the end of an XML document."
)

XML_NOOP_METHOD_2(
    xml_handler_method_start_element,
    "XMLEventHandler>>startElement:attributes:",
    name,
    attributes,
    "Handle an XML start element and its immutable attribute dictionary."
)

XML_NOOP_METHOD_1(
    xml_handler_method_end_element,
    "XMLEventHandler>>endElement:",
    name,
    "Handle an XML end element."
)

XML_NOOP_METHOD_1(
    xml_handler_method_characters,
    "XMLEventHandler>>characters:",
    characters,
    "Handle a possibly partial run of XML character data."
)

XML_NOOP_METHOD_2(
    xml_handler_method_processing_instruction,
    "XMLEventHandler>>processingInstruction:data:",
    target,
    data,
    "Handle an XML processing instruction."
)

XML_NOOP_METHOD_1(
    xml_handler_method_comment,
    "XMLEventHandler>>comment:",
    comment,
    "Handle an XML comment."
)

XML_NOOP_METHOD_0(
    xml_handler_method_start_cdata,
    "XMLEventHandler>>startCDATA",
    "startCDATA",
    "Handle the start of a CDATA section."
)

XML_NOOP_METHOD_0(
    xml_handler_method_end_cdata,
    "XMLEventHandler>>endCDATA",
    "endCDATA",
    "Handle the end of a CDATA section."
)

static LT_Method_Descriptor XMLEventHandler_methods[] = {
    {"startDocument", &xml_handler_method_start_document},
    {"endDocument", &xml_handler_method_end_document},
    {"startElement:attributes:", &xml_handler_method_start_element},
    {"endElement:", &xml_handler_method_end_element},
    {"characters:", &xml_handler_method_characters},
    {"processingInstruction:data:", &xml_handler_method_processing_instruction},
    {"comment:", &xml_handler_method_comment},
    {"startCDATA", &xml_handler_method_start_cdata},
    {"endCDATA", &xml_handler_method_end_cdata},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_XMLEventHandler) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "XMLEventHandler",
    .documentation = "Abstract no-op handler for XML parsing events.",
    .instance_size = sizeof(LT_XMLEventHandler),
    .class_flags = LT_CLASS_FLAG_ABSTRACT,
    .methods = XMLEventHandler_methods,
};

LT_DEFINE_PRIMITIVE(
    primitive_xml_parse,
    "parse",
    "(byte-vector event-handler)",
    "Parse an XML bytevector with Expat and send events to event-handler."
){
    LT_Value cursor = arguments;
    LT_ByteVector* bytes;
    LT_Value handler;
    XML_Parser parser;
    struct XMLParseContext context;
    size_t offset = 0;
    size_t length;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(cursor, bytes, LT_ByteVector*, LT_ByteVector_from_value);
    LT_OBJECT_ARG(cursor, handler);
    LT_ARG_END(cursor);
    if (!LT_Value_is_instance_of(handler, LT_STATIC_CLASS(LT_XMLEventHandler))){
        LT_type_error(handler, &LT_XMLEventHandler_class);
    }

    parser = XML_ParserCreate(NULL);
    if (parser == NULL){
        LT_error("Could not allocate Expat XML parser");
    }
    context.handler = handler;
    XML_SetUserData(parser, &context);
    XML_SetElementHandler(parser, xml_start_element, xml_end_element);
    XML_SetCharacterDataHandler(parser, xml_character_data);
    XML_SetProcessingInstructionHandler(parser, xml_processing_instruction);
    XML_SetCommentHandler(parser, xml_comment);
    XML_SetCdataSectionHandler(parser, xml_start_cdata, xml_end_cdata);
    length = LT_ByteVector_length(bytes);

    LT_UNWIND_PROTECT({
        (void)LT_SEND(handler, "startDocument");
        do {
            size_t remaining = length - offset;
            int chunk = remaining > 65536 ? 65536 : (int)remaining;
            int final = offset + (size_t)chunk == length;

            if (XML_Parse(
                parser,
                (char*)LT_ByteVector_bytes(bytes) + offset,
                chunk,
                final
            ) == XML_STATUS_ERROR){
                LT_error(LT_sprintf(
                    "XML parse error at line %lu, column %lu: %s",
                    XML_GetCurrentLineNumber(parser),
                    XML_GetCurrentColumnNumber(parser),
                    XML_ErrorString(XML_GetErrorCode(parser))
                ));
            }
            offset += (size_t)chunk;
        } while (offset < length);
        (void)LT_SEND(handler, "endDocument");
    }, {
        XML_ParserFree(parser);
    });

    return handler;
}

void ListTalk_xml_load(LT_Environment* environment){
    LT_Package* package = LT_Package_new("ListTalk-XML");

    LT_Environment_bind(
        environment,
        LT_Symbol_new_in(package, "XMLEventHandler"),
        LT_STATIC_CLASS(LT_XMLEventHandler),
        LT_ENV_BINDING_FLAG_CONSTANT
    );
    LT_Environment_bind(
        environment,
        LT_Symbol_new_in(package, "parse"),
        LT_Primitive_from_static(&primitive_xml_parse),
        LT_ENV_BINDING_FLAG_CONSTANT
    );
    LT_loader_provide(environment, "xml");
}
