/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/utils/ini.h>
#include <ListTalk/classes/Dictionary.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/error.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

typedef struct LT_INISection LT_INISection;

struct LT_INISection {
    char* name;
    LT_InlineHash entries;
    LT_INISection* next;
};

struct LT_INI {
    LT_INISection* sections;
    LT_INISection* last_section;
};

static char ini_empty_section_name[] = "";

typedef struct {
    const char* source_name;
    const char* bytes;
    const char* cursor;
    const char* end;
    unsigned int flags;
    int duplicate_policy;
    size_t line;
    LT_INI* ini;
    LT_INISection* section;
} INIParser;

static void ini_error(INIParser* parser, const char* message){
    LT_error(
        LT_sprintf(
            "%s:%zu: %s",
            parser->source_name == NULL ? "<ini>" : parser->source_name,
            parser->line,
            message
        )
    );
}

static char* ini_slice(const char* start, const char* end){
    char* result = GC_MALLOC_ATOMIC((size_t)(end - start) + 1);

    memcpy(result, start, (size_t)(end - start));
    result[end - start] = '\0';
    return result;
}

static const char* trim_left(const char* cursor, const char* end){
    while (cursor < end && isspace((unsigned char)*cursor)){
        cursor++;
    }
    return cursor;
}

static const char* trim_right(const char* start, const char* end){
    while (end > start && isspace((unsigned char)end[-1])){
        end--;
    }
    return end;
}

static LT_INI* ini_new(void){
    LT_INI* ini = GC_NEW(LT_INI);

    ini->sections = NULL;
    ini->last_section = NULL;
    return ini;
}

static LT_INISection* ini_section_new(char* name){
    LT_INISection* section = GC_NEW(LT_INISection);

    section->name = name;
    LT_InlineHash_init(&section->entries);
    section->next = NULL;
    return section;
}

static LT_INISection* ini_find_section(LT_INI* ini, const char* name){
    LT_INISection* section = ini->sections;
    const char* normalized = name == NULL ? "" : name;

    while (section != NULL){
        if (strcmp(section->name, normalized) == 0){
            return section;
        }
        section = section->next;
    }
    return NULL;
}

static LT_INISection* ini_get_section(LT_INI* ini, char* name){
    LT_INISection* section;

    section = ini_find_section(ini, name);
    if (section != NULL){
        return section;
    }

    section = ini_section_new(name == NULL ? ini_empty_section_name : name);
    if (ini->last_section == NULL){
        ini->sections = section;
    } else {
        ini->last_section->next = section;
    }
    ini->last_section = section;
    return section;
}

static void ini_put(INIParser* parser, char* key, char* value){
    if (parser->section == NULL){
        if ((parser->flags & LT_INI_ALLOW_GLOBAL_KEYS) == 0){
            ini_error(parser, "Key/value pair before section");
        }
        parser->section = ini_get_section(parser->ini, NULL);
    }

    if (LT_StringHash_at(&parser->section->entries, key) != NULL){
        if (parser->duplicate_policy == LT_INI_DUPLICATE_ERROR){
            ini_error(parser, "Duplicate INI key");
        }
        if (parser->duplicate_policy == LT_INI_DUPLICATE_FIRST_WINS){
            return;
        }
        /* LT_INI_DUPLICATE_LAST_WINS: fall through and overwrite */
    }
    LT_StringHash_at_put(&parser->section->entries, key, value);
}

static void ini_parse_section(INIParser* parser, const char* start, const char* end){
    const char* name_start;
    const char* name_end;

    end = trim_right(start, end);
    if (end <= start || end[-1] != ']'){
        ini_error(parser, "Invalid INI section header");
    }

    name_start = trim_left(start + 1, end - 1);
    name_end = trim_right(name_start, end - 1);
    if (name_start == name_end){
        ini_error(parser, "Empty INI section name");
    }
    parser->section = ini_get_section(parser->ini, ini_slice(name_start, name_end));
}

static const char* ini_find_separator(const char* start, const char* end){
    const char* cursor = start;

    while (cursor < end){
        if (*cursor == '=' || *cursor == ':'){
            return cursor;
        }
        cursor++;
    }
    return NULL;
}

static void ini_parse_key_value(INIParser* parser, const char* start, const char* end){
    const char* separator = ini_find_separator(start, end);
    const char* key_start;
    const char* key_end;
    const char* value_start;
    const char* value_end;

    if (separator == NULL){
        ini_error(parser, "Expected INI key/value separator");
    }

    key_start = trim_left(start, separator);
    key_end = trim_right(key_start, separator);
    if (key_start == key_end){
        ini_error(parser, "Empty INI key");
    }

    value_start = trim_left(separator + 1, end);
    value_end = trim_right(value_start, end);
    if (value_start == value_end
        && (parser->flags & LT_INI_ALLOW_EMPTY_VALUES) == 0){
        ini_error(parser, "Empty INI value");
    }

    ini_put(parser, ini_slice(key_start, key_end), ini_slice(value_start, value_end));
}

static void ini_parse_line(INIParser* parser, const char* start, const char* end){
    const char* trimmed_start = trim_left(start, end);
    const char* trimmed_end = trim_right(trimmed_start, end);

    if (trimmed_start == trimmed_end){
        return;
    }
    if (*trimmed_start == ';' || *trimmed_start == '#'){
        return;
    }
    if (*trimmed_start == '['){
        ini_parse_section(parser, trimmed_start, trimmed_end);
        return;
    }
    ini_parse_key_value(parser, trimmed_start, trimmed_end);
}

LT_INI* LT_INI_parseBytes(
    const char* source_name,
    const char* bytes,
    size_t length,
    unsigned int flags,
    int duplicate_policy
){
    INIParser parser;

    if (duplicate_policy != LT_INI_DUPLICATE_LAST_WINS
        && duplicate_policy != LT_INI_DUPLICATE_FIRST_WINS
        && duplicate_policy != LT_INI_DUPLICATE_ERROR){
        LT_error("Invalid INI duplicate policy");
    }

    parser.source_name = source_name;
    parser.bytes = bytes;
    parser.cursor = bytes;
    parser.end = bytes + length;
    parser.flags = flags;
    parser.duplicate_policy = duplicate_policy;
    parser.line = 1;
    parser.ini = ini_new();
    parser.section = NULL;

    while (parser.cursor < parser.end){
        const char* line_start = parser.cursor;
        const char* line_end;

        while (parser.cursor < parser.end
               && *parser.cursor != '\n'
               && *parser.cursor != '\r'){
            parser.cursor++;
        }
        line_end = parser.cursor;
        ini_parse_line(&parser, line_start, line_end);

        if (parser.cursor < parser.end && *parser.cursor == '\r'){
            parser.cursor++;
            if (parser.cursor < parser.end && *parser.cursor == '\n'){
                parser.cursor++;
            }
        } else if (parser.cursor < parser.end && *parser.cursor == '\n'){
            parser.cursor++;
        }
        parser.line++;
    }

    return parser.ini;
}

LT_INI* LT_INI_parseString(
    const char* source_name,
    LT_String* string,
    unsigned int flags,
    int duplicate_policy
){
    return LT_INI_parseBytes(
        source_name,
        LT_String_value_cstr(string),
        LT_String_byte_length(string),
        flags,
        duplicate_policy
    );
}

LT_INI* LT_INI_parseFile(
    const char* path,
    unsigned int flags,
    int duplicate_policy
){
    FILE* stream = fopen(path, "rb");
    LT_StringBuilder* builder;
    char buffer[4096];
    size_t count;

    if (stream == NULL){
        LT_system_error("Could not open INI file", errno);
    }

    builder = LT_StringBuilder_new();
    while ((count = fread(buffer, 1, sizeof(buffer), stream)) > 0){
        LT_StringBuilder_append_bytes(builder, buffer, count);
    }
    if (ferror(stream)){
        int errnum = errno;

        fclose(stream);
        LT_system_error("Could not read INI file", errnum);
    }
    fclose(stream);

    return LT_INI_parseBytes(
        path,
        LT_StringBuilder_value(builder),
        LT_StringBuilder_length(builder),
        flags,
        duplicate_policy
    );
}

int LT_INI_at(
    LT_INI* ini,
    const char* section_name,
    const char* key,
    const char** value_out
){
    LT_INISection* section = ini_find_section(ini, section_name);
    char* value;

    if (section == NULL){
        return 0;
    }

    value = LT_StringHash_at(&section->entries, (char*)key);
    if (value == NULL){
        return 0;
    }
    if (value_out != NULL){
        *value_out = value;
    }
    return 1;
}

static LT_Dictionary* ini_section_as_dictionary(LT_INISection* section){
    LT_Dictionary* dictionary = LT_Dictionary_new();
    LT_InlineHash* table = &section->entries;
    size_t i;

    LT_MutexWord_lock(&table->lock);
    for (i = 0; i < table->mask + 1; i++){
        LT_InlineHash_Entry* entry = table->vector[i];

        while (entry != NULL){
            LT_Dictionary_atPut(
                dictionary,
                (LT_Value)(uintptr_t)LT_String_new_cstr(entry->key),
                (LT_Value)(uintptr_t)LT_String_new_cstr(entry->value)
            );
            entry = entry->next;
        }
    }
    LT_MutexWord_unlock(&table->lock);
    return dictionary;
}

LT_Value LT_INI_asDictionary(LT_INI* ini){
    LT_Dictionary* dictionary = LT_Dictionary_new();
    LT_INISection* section = ini->sections;

    while (section != NULL){
        LT_Dictionary_atPut(
            dictionary,
            (LT_Value)(uintptr_t)LT_String_new_cstr(section->name),
            (LT_Value)(uintptr_t)ini_section_as_dictionary(section)
        );
        section = section->next;
    }
    return (LT_Value)(uintptr_t)dictionary;
}

LT_Value LT_INI_sectionNames(LT_INI* ini){
    LT_ListBuilder* builder = LT_ListBuilder_new();
    LT_INISection* section = ini->sections;

    while (section != NULL){
        LT_ListBuilder_append(
            builder,
            (LT_Value)(uintptr_t)LT_String_new_cstr(section->name)
        );
        section = section->next;
    }
    return LT_ListBuilder_value(builder);
}

LT_Value LT_INI_sectionAsDictionary(LT_INI* ini, const char* section_name){
    LT_INISection* section = ini_find_section(ini, section_name);

    if (section == NULL){
        return LT_NIL;
    }
    return (LT_Value)(uintptr_t)ini_section_as_dictionary(section);
}
