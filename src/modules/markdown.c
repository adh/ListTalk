/*
 * SPDX-License-Identifier: MIT
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/utils.h>

#include "markdown-config.h"

#include <limits.h>
#include <md4c.h>
#include <md4c-html.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef MD_DIALECT_COMMONMARK
#define MD_DIALECT_COMMONMARK 0
#endif

struct MarkdownParseStats {
    size_t event_count;
};

struct MarkdownHtmlOutput {
    LT_StringBuilder* builder;
};

struct MarkdownAstNode {
    LT_Value type;
    LT_Value properties;
    LT_ListBuilder* children;
    struct MarkdownAstNode* parent;
};

struct MarkdownAstBuilder {
    struct MarkdownAstNode* current;
    LT_Value root;
};

struct MarkdownOption {
    const char* name;
    unsigned int flag;
};

static const struct MarkdownOption parser_options[] = {
#ifdef MD_FLAG_COLLAPSEWHITESPACE
    {"collapse-whitespace", MD_FLAG_COLLAPSEWHITESPACE},
#endif
#ifdef MD_FLAG_PERMISSIVEATXHEADERS
    {"permissive-atx-headers", MD_FLAG_PERMISSIVEATXHEADERS},
#endif
#ifdef MD_FLAG_PERMISSIVEURLAUTOLINKS
    {"permissive-url-autolinks", MD_FLAG_PERMISSIVEURLAUTOLINKS},
#endif
#ifdef MD_FLAG_PERMISSIVEEMAILAUTOLINKS
    {"permissive-email-autolinks", MD_FLAG_PERMISSIVEEMAILAUTOLINKS},
#endif
#ifdef MD_FLAG_NOINDENTEDCODEBLOCKS
    {"no-indented-code-blocks", MD_FLAG_NOINDENTEDCODEBLOCKS},
#endif
#ifdef MD_FLAG_NOHTMLBLOCKS
    {"no-html-blocks", MD_FLAG_NOHTMLBLOCKS},
#endif
#ifdef MD_FLAG_NOHTMLSPANS
    {"no-html-spans", MD_FLAG_NOHTMLSPANS},
#endif
#ifdef MD_FLAG_TABLES
    {"tables", MD_FLAG_TABLES},
#endif
#ifdef MD_FLAG_STRIKETHROUGH
    {"strikethrough", MD_FLAG_STRIKETHROUGH},
#endif
#ifdef MD_FLAG_PERMISSIVEWWWAUTOLINKS
    {"permissive-www-autolinks", MD_FLAG_PERMISSIVEWWWAUTOLINKS},
#endif
#ifdef MD_FLAG_TASKLISTS
    {"task-lists", MD_FLAG_TASKLISTS},
#endif
#ifdef MD_FLAG_LATEXMATHSPANS
    {"latex-math-spans", MD_FLAG_LATEXMATHSPANS},
#endif
#ifdef MD_FLAG_WIKILINKS
    {"wiki-links", MD_FLAG_WIKILINKS},
#endif
#ifdef MD_FLAG_UNDERLINE
    {"underline", MD_FLAG_UNDERLINE},
#endif
#ifdef MD_FLAG_HARD_SOFT_BREAKS
    {"hard-soft-breaks", MD_FLAG_HARD_SOFT_BREAKS},
#endif
#ifdef MD_FLAG_PERMISSIVEAUTOLINKS
    {"permissive-autolinks", MD_FLAG_PERMISSIVEAUTOLINKS},
#endif
#ifdef MD_FLAG_NOHTML
    {"no-html", MD_FLAG_NOHTML},
#endif
#ifdef MD_DIALECT_GITHUB
    {"github", MD_DIALECT_GITHUB},
#endif
    {"commonmark", MD_DIALECT_COMMONMARK},
};

static const struct MarkdownOption html_renderer_options[] = {
#ifdef MD_HTML_FLAG_DEBUG
    {"debug", MD_HTML_FLAG_DEBUG},
#endif
#ifdef MD_HTML_FLAG_VERBATIM_ENTITIES
    {"verbatim-entities", MD_HTML_FLAG_VERBATIM_ENTITIES},
#endif
#ifdef MD_HTML_FLAG_SKIP_UTF8_BOM
    {"skip-utf8-bom", MD_HTML_FLAG_SKIP_UTF8_BOM},
#endif
#ifdef MD_HTML_FLAG_XHTML
    {"xhtml", MD_HTML_FLAG_XHTML},
#endif
};

static void bind_markdown_primitive(LT_Environment* environment,
                                    LT_Package* package,
                                    LT_Primitive* primitive){
    LT_Environment_bind(
        environment,
        LT_Symbol_new_in(package, primitive->name),
        LT_Primitive_from_static(primitive),
        LT_ENV_BINDING_FLAG_CONSTANT
    );
}

static LT_Value keyword(char* name){
    return LT_Symbol_new_in(LT_PACKAGE_KEYWORD, name);
}

static LT_Value lt_string_from_bytes(const MD_CHAR* text, MD_SIZE size){
    return (LT_Value)(uintptr_t)LT_String_new((char*)text, (size_t)size);
}

static LT_Value lt_string_from_attribute(MD_ATTRIBUTE* attribute){
    return lt_string_from_bytes(attribute->text, attribute->size);
}

static LT_Value lt_string_from_char(MD_CHAR ch){
    char buffer[1] = {ch};

    return (LT_Value)(uintptr_t)LT_String_new(buffer, 1);
}

static LT_Value boolean_value(int value){
    return value ? LT_TRUE : LT_FALSE;
}

static LT_Value plist0(void){
    return LT_NIL;
}

static LT_Value plist1(char* key, LT_Value value){
    return LT_listn(2, keyword(key), value);
}

static LT_Value plist2(char* key1, LT_Value value1, char* key2, LT_Value value2){
    return LT_listn(4, keyword(key1), value1, keyword(key2), value2);
}

static LT_Value plist3(char* key1,
                       LT_Value value1,
                       char* key2,
                       LT_Value value2,
                       char* key3,
                       LT_Value value3){
    return LT_listn(6, keyword(key1), value1, keyword(key2), value2, keyword(key3), value3);
}

static LT_Value supported_option_keywords(const struct MarkdownOption* options,
                                          size_t option_count){
    LT_ListBuilder* builder = LT_ListBuilder_new();
    size_t index;

    for (index = 0; index < option_count; index++){
        LT_ListBuilder_append(
            builder,
            LT_Symbol_new_in(LT_PACKAGE_KEYWORD, (char*)options[index].name)
        );
    }

    return LT_ImmutableList_fromList(LT_ListBuilder_value(builder));
}

static void bind_markdown_constant(LT_Environment* environment,
                                   LT_Package* package,
                                   const char* name,
                                   LT_Value value){
    LT_Environment_bind(
        environment,
        LT_Symbol_new_in(package, (char*)name),
        value,
        LT_ENV_BINDING_FLAG_CONSTANT
    );
}

static unsigned int option_flag_for_keyword(const struct MarkdownOption* options,
                                            size_t option_count,
                                            const char* keyword_name,
                                            const char* option_kind){
    size_t index;

    for (index = 0; index < option_count; index++){
        if (strcmp(keyword_name, options[index].name) == 0){
            return options[index].flag;
        }
    }

    LT_error(LT_sprintf("Unknown %s option: %s", option_kind, keyword_name));
    return 0;
}

static unsigned int options_from_keyword_list(const struct MarkdownOption* options,
                                              size_t option_count,
                                              LT_Value list,
                                              const char* option_kind){
    unsigned int flags = 0;
    LT_Value cursor = list;

    while (cursor != LT_NIL){
        LT_Value keyword;
        LT_Symbol* symbol;

        if (!LT_Pair_p(cursor)){
            LT_error(LT_sprintf("%s options must be a proper list", option_kind));
        }

        keyword = LT_car(cursor);
        symbol = LT_Symbol_from_value(keyword);
        if (LT_Symbol_package(symbol) != LT_PACKAGE_KEYWORD){
            LT_error(LT_sprintf("%s options must be keywords", option_kind));
        }

        flags |= option_flag_for_keyword(
            options,
            option_count,
            LT_Symbol_name(symbol),
            option_kind
        );
        cursor = LT_cdr(cursor);
    }

    return flags;
}

static unsigned int parser_options_from_list(LT_Value list){
    return options_from_keyword_list(
        parser_options,
        sizeof(parser_options) / sizeof(parser_options[0]),
        list,
        "Markdown parser"
    );
}

static unsigned int html_renderer_options_from_list(LT_Value list){
    return options_from_keyword_list(
        html_renderer_options,
        sizeof(html_renderer_options) / sizeof(html_renderer_options[0]),
        list,
        "HTML renderer"
    );
}

static char* block_type_name(MD_BLOCKTYPE type){
    switch (type){
    case MD_BLOCK_DOC:
        return "document";
    case MD_BLOCK_QUOTE:
        return "quote";
    case MD_BLOCK_UL:
        return "unordered-list";
    case MD_BLOCK_OL:
        return "ordered-list";
    case MD_BLOCK_LI:
        return "list-item";
    case MD_BLOCK_HR:
        return "thematic-break";
    case MD_BLOCK_H:
        return "heading";
    case MD_BLOCK_CODE:
        return "code-block";
    case MD_BLOCK_HTML:
        return "html-block";
    case MD_BLOCK_P:
        return "paragraph";
    case MD_BLOCK_TABLE:
        return "table";
    case MD_BLOCK_THEAD:
        return "table-head";
    case MD_BLOCK_TBODY:
        return "table-body";
    case MD_BLOCK_TR:
        return "table-row";
    case MD_BLOCK_TH:
        return "table-header-cell";
    case MD_BLOCK_TD:
        return "table-cell";
    }

    LT_error("Unknown md4c block type");
    return "unknown-block";
}

static char* span_type_name(MD_SPANTYPE type){
    switch (type){
    case MD_SPAN_EM:
        return "emphasis";
    case MD_SPAN_STRONG:
        return "strong";
    case MD_SPAN_A:
        return "link";
    case MD_SPAN_IMG:
        return "image";
    case MD_SPAN_CODE:
        return "code";
    case MD_SPAN_DEL:
        return "delete";
    case MD_SPAN_LATEXMATH:
        return "latex-math";
    case MD_SPAN_LATEXMATH_DISPLAY:
        return "latex-math-display";
    case MD_SPAN_WIKILINK:
        return "wiki-link";
    case MD_SPAN_U:
        return "underline";
    }

    LT_error("Unknown md4c span type");
    return "unknown-span";
}

static LT_Value alignment_keyword(MD_ALIGN align){
    switch (align){
    case MD_ALIGN_DEFAULT:
        return keyword("default");
    case MD_ALIGN_LEFT:
        return keyword("left");
    case MD_ALIGN_CENTER:
        return keyword("center");
    case MD_ALIGN_RIGHT:
        return keyword("right");
    }

    LT_error("Unknown md4c alignment");
    return keyword("default");
}

static LT_Value block_properties(MD_BLOCKTYPE type, void* detail){
    switch (type){
    case MD_BLOCK_UL: {
        MD_BLOCK_UL_DETAIL* ul = detail;

        return plist2(
            "tight?",
            boolean_value(ul->is_tight),
            "mark",
            lt_string_from_char(ul->mark)
        );
    }
    case MD_BLOCK_OL: {
        MD_BLOCK_OL_DETAIL* ol = detail;

        return plist3(
            "start",
            LT_Integer_from_uintmax(ol->start),
            "tight?",
            boolean_value(ol->is_tight),
            "mark-delimiter",
            lt_string_from_char(ol->mark_delimiter)
        );
    }
    case MD_BLOCK_LI: {
        MD_BLOCK_LI_DETAIL* li = detail;

        if (!li->is_task){
            return plist1("task?", LT_FALSE);
        }

        return plist3(
            "task?",
            LT_TRUE,
            "task-mark",
            lt_string_from_char(li->task_mark),
            "task-mark-offset",
            LT_Integer_from_uintmax(li->task_mark_offset)
        );
    }
    case MD_BLOCK_H: {
        MD_BLOCK_H_DETAIL* heading = detail;

        return plist1("level", LT_Integer_from_uintmax(heading->level));
    }
    case MD_BLOCK_CODE: {
        MD_BLOCK_CODE_DETAIL* code = detail;

        return plist3(
            "info",
            lt_string_from_attribute(&code->info),
            "lang",
            lt_string_from_attribute(&code->lang),
            "fence-char",
            code->fence_char == 0 ? LT_NIL : lt_string_from_char(code->fence_char)
        );
    }
    case MD_BLOCK_TABLE: {
        MD_BLOCK_TABLE_DETAIL* table = detail;

        return plist3(
            "columns",
            LT_Integer_from_uintmax(table->col_count),
            "head-rows",
            LT_Integer_from_uintmax(table->head_row_count),
            "body-rows",
            LT_Integer_from_uintmax(table->body_row_count)
        );
    }
    case MD_BLOCK_TH:
    case MD_BLOCK_TD: {
        MD_BLOCK_TD_DETAIL* cell = detail;

        return plist1("align", alignment_keyword(cell->align));
    }
    default:
        return plist0();
    }
}

static LT_Value span_properties(MD_SPANTYPE type, void* detail){
    switch (type){
    case MD_SPAN_A: {
        MD_SPAN_A_DETAIL* link = detail;

#if HAVE_MD_SPAN_A_DETAIL_IS_AUTOLINK
        return plist3(
            "href",
            lt_string_from_attribute(&link->href),
            "title",
            lt_string_from_attribute(&link->title),
            "autolink?",
            boolean_value(link->is_autolink)
        );
#else
        return plist2(
            "href",
            lt_string_from_attribute(&link->href),
            "title",
            lt_string_from_attribute(&link->title)
        );
#endif
    }
    case MD_SPAN_IMG: {
        MD_SPAN_IMG_DETAIL* image = detail;

        return plist2(
            "src",
            lt_string_from_attribute(&image->src),
            "title",
            lt_string_from_attribute(&image->title)
        );
    }
    case MD_SPAN_WIKILINK: {
        MD_SPAN_WIKILINK_DETAIL* wikilink = detail;

        return plist1("target", lt_string_from_attribute(&wikilink->target));
    }
    default:
        return plist0();
    }
}

static int count_block_event(MD_BLOCKTYPE type, void* detail, void* userdata){
    struct MarkdownParseStats* stats = userdata;

    (void)type;
    (void)detail;
    stats->event_count++;
    return 0;
}

static int count_span_event(MD_SPANTYPE type, void* detail, void* userdata){
    struct MarkdownParseStats* stats = userdata;

    (void)type;
    (void)detail;
    stats->event_count++;
    return 0;
}

static int count_text_event(MD_TEXTTYPE type,
                            const MD_CHAR* text,
                            MD_SIZE size,
                            void* userdata){
    struct MarkdownParseStats* stats = userdata;

    (void)type;
    (void)text;
    (void)size;
    stats->event_count++;
    return 0;
}

static struct MarkdownAstNode* ast_node_new(LT_Value type,
                                            LT_Value properties,
                                            struct MarkdownAstNode* parent){
    struct MarkdownAstNode* node = GC_NEW(struct MarkdownAstNode);

    node->type = type;
    node->properties = properties;
    node->children = LT_ListBuilder_new();
    node->parent = parent;
    return node;
}

static LT_Value ast_node_value(struct MarkdownAstNode* node){
    LT_ListBuilder* builder = LT_ListBuilder_new();

    LT_ListBuilder_append(builder, node->type);
    LT_ListBuilder_append(builder, node->properties);
    return LT_ListBuilder_valueWithRest(builder, LT_ListBuilder_value(node->children));
}

static void ast_append_child(struct MarkdownAstBuilder* ast, LT_Value child){
    if (ast->current == NULL){
        LT_error("Markdown parser produced content outside the document node");
    }

    LT_ListBuilder_append(ast->current->children, child);
}

static void ast_enter_node(struct MarkdownAstBuilder* ast,
                           LT_Value type,
                           LT_Value properties){
    ast->current = ast_node_new(type, properties, ast->current);
}

static void ast_leave_node(struct MarkdownAstBuilder* ast){
    struct MarkdownAstNode* node = ast->current;
    LT_Value value;

    if (node == NULL){
        LT_error("Markdown parser left a node with an empty AST stack");
    }

    value = ast_node_value(node);
    ast->current = node->parent;
    if (ast->current == NULL){
        ast->root = value;
    } else {
        ast_append_child(ast, value);
    }
}

static LT_Value text_node(char* type, const MD_CHAR* text, MD_SIZE size){
    return LT_listn(3, keyword(type), LT_NIL, lt_string_from_bytes(text, size));
}

static LT_Value ast_text_value(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size){
    switch (type){
    case MD_TEXT_NORMAL:
    case MD_TEXT_CODE:
        return lt_string_from_bytes(text, size);
    case MD_TEXT_NULLCHAR:
        return LT_listn(2, keyword("null-character"), LT_NIL);
    case MD_TEXT_BR:
        return LT_listn(2, keyword("line-break"), LT_NIL);
    case MD_TEXT_SOFTBR:
        return LT_listn(2, keyword("soft-break"), LT_NIL);
    case MD_TEXT_ENTITY:
        return text_node("entity", text, size);
    case MD_TEXT_HTML:
        return text_node("html", text, size);
    case MD_TEXT_LATEXMATH:
        return text_node("latex-math-text", text, size);
    }

    LT_error("Unknown md4c text type");
    return LT_NIL;
}

static int ast_enter_block(MD_BLOCKTYPE type, void* detail, void* userdata){
    struct MarkdownAstBuilder* ast = userdata;

    ast_enter_node(ast, keyword(block_type_name(type)), block_properties(type, detail));
    return 0;
}

static int ast_leave_block(MD_BLOCKTYPE type, void* detail, void* userdata){
    struct MarkdownAstBuilder* ast = userdata;

    (void)type;
    (void)detail;
    ast_leave_node(ast);
    return 0;
}

static int ast_enter_span(MD_SPANTYPE type, void* detail, void* userdata){
    struct MarkdownAstBuilder* ast = userdata;

    ast_enter_node(ast, keyword(span_type_name(type)), span_properties(type, detail));
    return 0;
}

static int ast_leave_span(MD_SPANTYPE type, void* detail, void* userdata){
    struct MarkdownAstBuilder* ast = userdata;

    (void)type;
    (void)detail;
    ast_leave_node(ast);
    return 0;
}

static int ast_text(MD_TEXTTYPE type,
                    const MD_CHAR* text,
                    MD_SIZE size,
                    void* userdata){
    struct MarkdownAstBuilder* ast = userdata;

    ast_append_child(ast, ast_text_value(type, text, size));
    return 0;
}

static MD_PARSER event_count_parser(unsigned int parser_flags){
    MD_PARSER parser = {
        .abi_version = 0,
        .flags = parser_flags,
        .enter_block = count_block_event,
        .leave_block = count_block_event,
        .enter_span = count_span_event,
        .leave_span = count_span_event,
        .text = count_text_event,
        .debug_log = NULL,
        .syntax = NULL
    };

    return parser;
}

static MD_PARSER ast_parser(unsigned int parser_flags){
    MD_PARSER parser = {
        .abi_version = 0,
        .flags = parser_flags,
        .enter_block = ast_enter_block,
        .leave_block = ast_leave_block,
        .enter_span = ast_enter_span,
        .leave_span = ast_leave_span,
        .text = ast_text,
        .debug_log = NULL,
        .syntax = NULL
    };

    return parser;
}

static int parse_markdown_events(LT_String* markdown,
                                 unsigned int parser_flags,
                                 struct MarkdownParseStats* stats){
    MD_PARSER parser = event_count_parser(parser_flags);
    size_t byte_length = LT_String_byte_length(markdown);

    if (byte_length > UINT_MAX){
        LT_error("Markdown input is too large for md4c");
    }

    return md_parse(
        LT_String_value_cstr(markdown),
        (MD_SIZE)byte_length,
        &parser,
        stats
    );
}

static LT_Value parse_markdown_ast(LT_String* markdown, unsigned int parser_flags){
    MD_PARSER parser = ast_parser(parser_flags);
    struct MarkdownAstBuilder ast = {
        .current = NULL,
        .root = LT_NIL
    };
    size_t byte_length = LT_String_byte_length(markdown);

    if (byte_length > UINT_MAX){
        LT_error("Markdown input is too large for md4c");
    }

    if (md_parse(
        LT_String_value_cstr(markdown),
        (MD_SIZE)byte_length,
        &parser,
        &ast
    ) != 0){
        LT_error("md4c failed to parse Markdown");
    }
    if (ast.current != NULL){
        LT_error("Markdown parser ended with an incomplete AST stack");
    }

    return ast.root;
}

static void append_html_output(const MD_CHAR* text, MD_SIZE size, void* userdata){
    struct MarkdownHtmlOutput* output = userdata;

    LT_StringBuilder_append_bytes(output->builder, text, size);
}

static LT_String* markdown_to_html(LT_String* markdown,
                                   unsigned int parser_flags,
                                   unsigned int renderer_flags){
    struct MarkdownHtmlOutput output = {
        .builder = LT_StringBuilder_new()
    };
    size_t byte_length = LT_String_byte_length(markdown);

    if (byte_length > UINT_MAX){
        LT_error("Markdown input is too large for md4c");
    }

    if (md_html(
        LT_String_value_cstr(markdown),
        (MD_SIZE)byte_length,
        append_html_output,
        &output,
        parser_flags,
        renderer_flags
    ) != 0){
        LT_error("md4c failed to render Markdown as HTML");
    }

    return LT_String_new(
        LT_StringBuilder_value(output.builder),
        LT_StringBuilder_length(output.builder)
    );
}

LT_DEFINE_PRIMITIVE(
    primitive_markdown_parse,
    "parse",
    "(markdown :optional parser-options)",
    "Parse Markdown with md4c and return an S-expression AST."
){
    LT_String* markdown;
    LT_Value parser_option_list;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(arguments, markdown, LT_String*, LT_String_from_value);
    LT_OBJECT_ARG_OPT(arguments, parser_option_list, LT_NIL);
    LT_ARG_END(arguments);

    return parse_markdown_ast(
        markdown,
        parser_options_from_list(parser_option_list)
    );
}

LT_DEFINE_PRIMITIVE(
    primitive_markdown_valid_p,
    "valid?",
    "(markdown parser-options)",
    "Parse Markdown with md4c and return true when parsing succeeds."
){
    LT_String* markdown;
    LT_Value parser_option_list;
    struct MarkdownParseStats stats = {0};

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(arguments, markdown, LT_String*, LT_String_from_value);
    LT_OBJECT_ARG_OPT(arguments, parser_option_list, LT_NIL);
    LT_ARG_END(arguments);

    if (parse_markdown_events(
        markdown,
        parser_options_from_list(parser_option_list),
        &stats
    ) != 0){
        LT_error("md4c failed to parse Markdown");
    }

    return LT_TRUE;
}

LT_DEFINE_PRIMITIVE(
    primitive_markdown_event_count,
    "event-count",
    "(markdown parser-options)",
    "Parse Markdown with md4c and return the number of parser callback events."
){
    LT_String* markdown;
    LT_Value parser_option_list;
    struct MarkdownParseStats stats = {0};

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(arguments, markdown, LT_String*, LT_String_from_value);
    LT_OBJECT_ARG_OPT(arguments, parser_option_list, LT_NIL);
    LT_ARG_END(arguments);

    if (parse_markdown_events(
        markdown,
        parser_options_from_list(parser_option_list),
        &stats
    ) != 0){
        LT_error("md4c failed to parse Markdown");
    }

    return LT_Integer_from_uintmax((uintmax_t)stats.event_count);
}

LT_DEFINE_PRIMITIVE(
    primitive_markdown_to_html,
    "to-html",
    "(markdown :optional parser-options renderer-options)",
    "Render Markdown to HTML with md4c-html."
){
    LT_String* markdown;
    LT_Value parser_option_list;
    LT_Value renderer_option_list;

    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_GENERIC_ARG(arguments, markdown, LT_String*, LT_String_from_value);
    LT_OBJECT_ARG_OPT(arguments, parser_option_list, LT_NIL);
    LT_OBJECT_ARG_OPT(arguments, renderer_option_list, LT_NIL);
    LT_ARG_END(arguments);

    return (LT_Value)(uintptr_t)markdown_to_html(
        markdown,
        parser_options_from_list(parser_option_list),
        html_renderer_options_from_list(renderer_option_list)
    );
}

void ListTalk_markdown_load(LT_Environment* environment){
    LT_Package* package = LT_Package_new("ListTalk:markdown");

    bind_markdown_constant(
        environment,
        package,
        "parser-options",
        supported_option_keywords(
            parser_options,
            sizeof(parser_options) / sizeof(parser_options[0])
        )
    );
    bind_markdown_constant(
        environment,
        package,
        "html-renderer-options",
        supported_option_keywords(
            html_renderer_options,
            sizeof(html_renderer_options) / sizeof(html_renderer_options[0])
        )
    );
    bind_markdown_primitive(environment, package, &primitive_markdown_parse);
    bind_markdown_primitive(environment, package, &primitive_markdown_valid_p);
    bind_markdown_primitive(environment, package, &primitive_markdown_event_count);
    bind_markdown_primitive(environment, package, &primitive_markdown_to_html);
    LT_loader_provide(environment, "markdown");
}
