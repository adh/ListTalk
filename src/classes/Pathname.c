/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */
#include <ListTalk/classes/Class.h>
#include <ListTalk/classes/Object.h>
#include <ListTalk/classes/Instant.h>
#include <ListTalk/classes/Integer.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Pathname.h>
#include <ListTalk/classes/Primitive.h>
#include <ListTalk/macros/arg_macros.h>
#include <ListTalk/vm/error.h>

#include <gc.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct LT_Pathname_s {
    LT_Object base;
    char* pathname;
};

struct LT_RelativePathname_s {
    LT_Pathname base;
};

struct LT_AbsolutePathname_s {
    LT_Pathname base;
};

struct LT_PathnameStat_s {
    LT_Object base;
    LT_Value pathname;
    struct stat status;
};

static LT_Value Pathname_boolean(int value){
    return value ? LT_TRUE : LT_FALSE;
}

static int Pathname_stat_buffer(LT_Pathname* pathname,
                                struct stat* status,
                                int follow_links,
                                int missing_is_false){
    int result = follow_links
        ? stat(LT_Pathname_value_cstr(pathname), status)
        : lstat(LT_Pathname_value_cstr(pathname), status);

    if (result == 0){
        return 1;
    }
    if (missing_is_false && (errno == ENOENT || errno == ENOTDIR)){
        return 0;
    }
    LT_system_error("Could not stat pathname", errno);
    return 0;
}

static LT_Value Pathname_instant(time_t seconds){
    return LT_Instant_new(LT_Number_multiply2(
        LT_Integer_from_intmax((intmax_t)seconds),
        LT_SmallInteger_new(1000000)
    ));
}

static void Pathname_check_string(LT_String* string){
    if (strlen(LT_String_value_cstr(string)) != LT_String_byte_length(string)){
        LT_error("Pathname cannot contain NUL bytes");
    }
}

static char* Pathname_normalize(const char* input, int absolute){
    size_t input_length = strlen(input);
    char* copy = GC_MALLOC_ATOMIC(input_length + 1);
    char** segments = GC_MALLOC(sizeof(char*) * (input_length + 1));
    size_t count = 0;
    char* cursor;
    char* output = GC_MALLOC_ATOMIC(input_length + 3);
    char* output_cursor = output;
    size_t index;

    memcpy(copy, input, input_length + 1);
    cursor = copy;
    while (*cursor != '\0'){
        char* segment;

        while (*cursor == '/'){
            cursor++;
        }
        if (*cursor == '\0'){
            break;
        }
        segment = cursor;
        while (*cursor != '\0' && *cursor != '/'){
            cursor++;
        }
        if (*cursor != '\0'){
            *cursor++ = '\0';
        }
        if (strcmp(segment, ".") == 0){
            continue;
        }
        if (strcmp(segment, "..") == 0){
            if (count > 0 && strcmp(segments[count - 1], "..") != 0){
                count--;
            } else if (absolute){
                LT_error("Absolute pathname cannot resolve above root");
            } else {
                segments[count++] = segment;
            }
        } else {
            segments[count++] = segment;
        }
    }

    if (absolute){
        *output_cursor++ = '/';
    } else if (count == 0){
        *output_cursor++ = '.';
    } else if (strcmp(segments[0], "..") != 0){
        *output_cursor++ = '.';
        *output_cursor++ = '/';
    }
    for (index = 0; index < count; index++){
        size_t length = strlen(segments[index]);

        if (index > 0){
            *output_cursor++ = '/';
        }
        memcpy(output_cursor, segments[index], length);
        output_cursor += length;
    }
    *output_cursor = '\0';
    return output;
}

static LT_Pathname* Pathname_allocate(LT_Class* klass, char* normalized){
    LT_Pathname* pathname = LT_Class_alloc(klass);

    pathname->pathname = normalized;
    return pathname;
}

static size_t Pathname_hash(LT_Value value){
    const unsigned char* cursor = (const unsigned char*)LT_Pathname_value_cstr(
        LT_Pathname_from_value(value)
    );
    uint32_t hash = UINT32_C(0x811c9dc5);

    while (*cursor != '\0'){
        hash ^= *cursor++;
        hash *= UINT32_C(0x01000193);
    }
    return (size_t)hash;
}

static int Pathname_equal_p(LT_Value left, LT_Value right){
    return LT_Pathname_p(right)
        && strcmp(
            LT_Pathname_value_cstr(LT_Pathname_from_value(left)),
            LT_Pathname_value_cstr(LT_Pathname_from_value(right))
        ) == 0;
}

static void Pathname_debugPrintOn(LT_Value value, FILE* stream){
    LT_String* string = LT_Pathname_as_string(LT_Pathname_from_value(value));

    fputs("#p", stream);
    LT_Value_debugPrintOn((LT_Value)(uintptr_t)string, stream);
}

typedef LT_Pathname* (*Pathname_StringConstructor)(LT_String* string);

static LT_Pathname* RelativePathname_from_string_as_pathname(LT_String* string){
    return (LT_Pathname*)LT_RelativePathname_from_string(string);
}

static LT_Pathname* AbsolutePathname_from_string_as_pathname(LT_String* string){
    return (LT_Pathname*)LT_AbsolutePathname_from_string(string);
}

static LT_Value Pathname_class_from_string(LT_Value arguments,
                                           LT_Class* expected_class,
                                           Pathname_StringConstructor constructor){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* string;

    LT_OBJECT_ARG(cursor, self);
    LT_GENERIC_ARG(cursor, string, LT_String*, LT_String_from_value);
    LT_ARG_END(cursor);
    if (self != (LT_Value)(uintptr_t)expected_class){
        LT_error("fromString: is not supported on this class");
    }
    return (LT_Value)(uintptr_t)constructor(string);
}

LT_DEFINE_PRIMITIVE(
    pathname_class_method_from_string,
    "Pathname class>>fromString:",
    "(self string)",
    "Create an absolute or relative pathname according to the string's shape."
){
    (void)tail_call_unwind_marker;
    return Pathname_class_from_string(
        arguments, &LT_Pathname_class, LT_Pathname_from_string
    );
}

LT_DEFINE_PRIMITIVE(
    relative_pathname_class_method_from_string,
    "RelativePathname class>>fromString:",
    "(self string)",
    "Create a relative pathname; reject a leading slash."
){
    (void)tail_call_unwind_marker;
    return Pathname_class_from_string(
        arguments,
        &LT_RelativePathname_class,
        RelativePathname_from_string_as_pathname
    );
}

LT_DEFINE_PRIMITIVE(
    absolute_pathname_class_method_from_string,
    "AbsolutePathname class>>fromString:",
    "(self string)",
    "Create an absolute pathname, adding a leading slash when absent."
){
    (void)tail_call_unwind_marker;
    return Pathname_class_from_string(
        arguments,
        &LT_AbsolutePathname_class,
        AbsolutePathname_from_string_as_pathname
    );
}

LT_DEFINE_PRIMITIVE(
    pathname_method_absolute_p,
    "Pathname>>absolute?",
    "(self)",
    "Return true when the pathname is absolute."
){
    LT_Value cursor = arguments;
    LT_Pathname* pathname;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, pathname, LT_Pathname*, LT_Pathname_from_value);
    LT_ARG_END(cursor);
    return LT_Pathname_absolute_p(pathname) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    pathname_method_relative_p,
    "Pathname>>relative?",
    "(self)",
    "Return true when the pathname is relative."
){
    LT_Value cursor = arguments;
    LT_Pathname* pathname;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, pathname, LT_Pathname*, LT_Pathname_from_value);
    LT_ARG_END(cursor);
    return LT_Pathname_relative_p(pathname) ? LT_TRUE : LT_FALSE;
}

LT_DEFINE_PRIMITIVE(
    pathname_method_append,
    "Pathname>>/",
    "(self pathname)",
    "Append a relative pathname to the receiver and normalize the result."
){
    LT_Value cursor = arguments;
    LT_Pathname* left;
    LT_Pathname* right;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, left, LT_Pathname*, LT_Pathname_from_value);
    LT_GENERIC_ARG(cursor, right, LT_Pathname*, LT_Pathname_from_value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_Pathname_append(left, right);
}

LT_DEFINE_PRIMITIVE(
    pathname_method_as_string,
    "Pathname>>asString",
    "(self)",
    "Return the pathname as a string."
){
    LT_Value cursor = arguments;
    LT_Pathname* pathname;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, pathname, LT_Pathname*, LT_Pathname_from_value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_Pathname_as_string(pathname);
}

LT_DEFINE_PRIMITIVE(
    pathname_method_parent,
    "Pathname>>parent",
    "(self)",
    "Return the pathname with its final segment removed."
){
    LT_Value cursor = arguments;
    LT_Pathname* pathname;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, pathname, LT_Pathname*, LT_Pathname_from_value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_Pathname_parent(pathname);
}

LT_DEFINE_PRIMITIVE(
    absolute_pathname_method_rooted_at,
    "AbsolutePathname>>rootedAt:",
    "(self root)",
    "Interpret the absolute pathname relative to root."
){
    LT_Value cursor = arguments;
    LT_AbsolutePathname* pathname;
    LT_Pathname* root;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, pathname, LT_AbsolutePathname*,
                   LT_AbsolutePathname_from_value);
    LT_GENERIC_ARG(cursor, root, LT_Pathname*, LT_Pathname_from_value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_AbsolutePathname_rooted_at(pathname, root);
}

#define DEFINE_PATHNAME_PREDICATE_METHOD(c_name, selector, function, description) \
    LT_DEFINE_PRIMITIVE(                                                          \
        c_name,                                                                   \
        "Pathname>>" selector,                                                   \
        "(self)",                                                                \
        description                                                               \
    ){                                                                            \
        LT_Value cursor = arguments;                                              \
        LT_Pathname* pathname;                                                    \
        (void)tail_call_unwind_marker;                                            \
        LT_GENERIC_ARG(cursor, pathname, LT_Pathname*, LT_Pathname_from_value);   \
        LT_ARG_END(cursor);                                                       \
        return Pathname_boolean(function(pathname));                              \
    }

DEFINE_PATHNAME_PREDICATE_METHOD(pathname_method_exists_p, "exists?",
    LT_Pathname_exists_p, "Return true when the pathname exists.")
DEFINE_PATHNAME_PREDICATE_METHOD(pathname_method_directory_p, "isDirectory?",
    LT_Pathname_directory_p, "Return true when the pathname names a directory.")
DEFINE_PATHNAME_PREDICATE_METHOD(pathname_method_regular_file_p, "isRegularFile?",
    LT_Pathname_regular_file_p, "Return true when the pathname names a regular file.")
DEFINE_PATHNAME_PREDICATE_METHOD(pathname_method_symbolic_link_p, "isSymbolicLink?",
    LT_Pathname_symbolic_link_p, "Return true when the pathname names a symbolic link.")
DEFINE_PATHNAME_PREDICATE_METHOD(pathname_method_fifo_p, "isFIFO?",
    LT_Pathname_fifo_p, "Return true when the pathname names a FIFO.")
DEFINE_PATHNAME_PREDICATE_METHOD(pathname_method_socket_p, "isSocket?",
    LT_Pathname_socket_p, "Return true when the pathname names a socket.")
DEFINE_PATHNAME_PREDICATE_METHOD(pathname_method_character_device_p,
    "isCharacterDevice?", LT_Pathname_character_device_p,
    "Return true when the pathname names a character device.")
DEFINE_PATHNAME_PREDICATE_METHOD(pathname_method_block_device_p, "isBlockDevice?",
    LT_Pathname_block_device_p, "Return true when the pathname names a block device.")
DEFINE_PATHNAME_PREDICATE_METHOD(pathname_method_readable_p, "readable?",
    LT_Pathname_readable_p, "Return true when the pathname is readable.")
DEFINE_PATHNAME_PREDICATE_METHOD(pathname_method_writable_p, "writable?",
    LT_Pathname_writable_p, "Return true when the pathname is writable.")
DEFINE_PATHNAME_PREDICATE_METHOD(pathname_method_executable_p, "executable?",
    LT_Pathname_executable_p, "Return true when the pathname is executable.")

LT_DEFINE_PRIMITIVE(
    pathname_method_stat,
    "Pathname>>stat",
    "(self)",
    "Return a snapshot of POSIX metadata for the pathname."
){
    LT_Value cursor = arguments;
    LT_Pathname* pathname;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, pathname, LT_Pathname*, LT_Pathname_from_value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_Pathname_stat(pathname);
}

LT_DEFINE_PRIMITIVE(
    pathname_method_lstat,
    "Pathname>>lstat",
    "(self)",
    "Return a POSIX metadata snapshot without following the final symlink."
){
    LT_Value cursor = arguments;
    LT_Pathname* pathname;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, pathname, LT_Pathname*, LT_Pathname_from_value);
    LT_ARG_END(cursor);
    return (LT_Value)(uintptr_t)LT_Pathname_lstat(pathname);
}

#define DEFINE_PATHNAME_STAT_PREDICATE(c_name, selector, predicate)              \
    LT_DEFINE_PRIMITIVE(c_name, "PathnameStat>>" selector, "(self)",            \
                        "Inspect the snapshotted POSIX file kind."){             \
        LT_Value cursor = arguments;                                             \
        LT_PathnameStat* self;                                                   \
        (void)tail_call_unwind_marker;                                           \
        LT_GENERIC_ARG(cursor, self, LT_PathnameStat*, LT_PathnameStat_from_value); \
        LT_ARG_END(cursor);                                                      \
        return Pathname_boolean(predicate(self->status.st_mode));                \
    }

DEFINE_PATHNAME_STAT_PREDICATE(pathname_stat_method_directory_p,
    "isDirectory?", S_ISDIR)
DEFINE_PATHNAME_STAT_PREDICATE(pathname_stat_method_regular_file_p,
    "isRegularFile?", S_ISREG)
DEFINE_PATHNAME_STAT_PREDICATE(pathname_stat_method_symbolic_link_p,
    "isSymbolicLink?", S_ISLNK)
DEFINE_PATHNAME_STAT_PREDICATE(pathname_stat_method_fifo_p, "isFIFO?", S_ISFIFO)
DEFINE_PATHNAME_STAT_PREDICATE(pathname_stat_method_socket_p, "isSocket?", S_ISSOCK)
DEFINE_PATHNAME_STAT_PREDICATE(pathname_stat_method_character_device_p,
    "isCharacterDevice?", S_ISCHR)
DEFINE_PATHNAME_STAT_PREDICATE(pathname_stat_method_block_device_p,
    "isBlockDevice?", S_ISBLK)

#define DEFINE_PATHNAME_STAT_INTEGER(c_name, selector, field, signedness)         \
    LT_DEFINE_PRIMITIVE(c_name, "PathnameStat>>" selector, "(self)",             \
                        "Return a snapshotted POSIX metadata field."){            \
        LT_Value cursor = arguments;                                             \
        LT_PathnameStat* self;                                                   \
        (void)tail_call_unwind_marker;                                           \
        LT_GENERIC_ARG(cursor, self, LT_PathnameStat*, LT_PathnameStat_from_value); \
        LT_ARG_END(cursor);                                                      \
        return LT_Integer_from_##signedness((signedness##_t)self->status.field);  \
    }

DEFINE_PATHNAME_STAT_INTEGER(pathname_stat_method_size, "size", st_size, intmax)
DEFINE_PATHNAME_STAT_INTEGER(pathname_stat_method_mode, "mode", st_mode, uintmax)
DEFINE_PATHNAME_STAT_INTEGER(pathname_stat_method_uid, "uid", st_uid, uintmax)
DEFINE_PATHNAME_STAT_INTEGER(pathname_stat_method_gid, "gid", st_gid, uintmax)
DEFINE_PATHNAME_STAT_INTEGER(pathname_stat_method_dev, "dev", st_dev, uintmax)
DEFINE_PATHNAME_STAT_INTEGER(pathname_stat_method_ino, "ino", st_ino, uintmax)
DEFINE_PATHNAME_STAT_INTEGER(pathname_stat_method_nlink, "nlink", st_nlink, uintmax)
DEFINE_PATHNAME_STAT_INTEGER(pathname_stat_method_rdev, "rdev", st_rdev, uintmax)
DEFINE_PATHNAME_STAT_INTEGER(pathname_stat_method_blksize, "blksize", st_blksize, intmax)
DEFINE_PATHNAME_STAT_INTEGER(pathname_stat_method_blocks, "blocks", st_blocks, intmax)

LT_DEFINE_PRIMITIVE(pathname_stat_method_device_p, "PathnameStat>>device?",
                    "(self)", "Return true for a character or block device."){
    LT_Value cursor = arguments;
    LT_PathnameStat* self;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, self, LT_PathnameStat*, LT_PathnameStat_from_value);
    LT_ARG_END(cursor);
    return Pathname_boolean(
        S_ISCHR(self->status.st_mode) || S_ISBLK(self->status.st_mode)
    );
}

LT_DEFINE_PRIMITIVE(pathname_stat_method_pathname, "PathnameStat>>pathname",
                    "(self)", "Return the pathname used for this snapshot."){
    LT_Value cursor = arguments;
    LT_PathnameStat* self;
    (void)tail_call_unwind_marker;
    LT_GENERIC_ARG(cursor, self, LT_PathnameStat*, LT_PathnameStat_from_value);
    LT_ARG_END(cursor);
    return self->pathname;
}

#define DEFINE_PATHNAME_STAT_TIME(c_name, selector, field)                       \
    LT_DEFINE_PRIMITIVE(c_name, "PathnameStat>>" selector, "(self)",             \
                        "Return a snapshotted POSIX timestamp."){                 \
        LT_Value cursor = arguments;                                             \
        LT_PathnameStat* self;                                                   \
        (void)tail_call_unwind_marker;                                           \
        LT_GENERIC_ARG(cursor, self, LT_PathnameStat*, LT_PathnameStat_from_value); \
        LT_ARG_END(cursor);                                                      \
        return Pathname_instant(self->status.field);                             \
    }

DEFINE_PATHNAME_STAT_TIME(pathname_stat_method_accessed_at, "accessedAt", st_atime)
DEFINE_PATHNAME_STAT_TIME(pathname_stat_method_modified_at, "modifiedAt", st_mtime)
DEFINE_PATHNAME_STAT_TIME(pathname_stat_method_status_changed_at,
    "statusChangedAt", st_ctime)

static LT_PathnameStat* PathnameStat_new_for_value(LT_Value path,
                                                   int follow_links){
    LT_PathnameStat* result = LT_Class_ALLOC(LT_PathnameStat);
    const char* bytes = LT_Pathname_like_value_cstr(path);
    int status = follow_links
        ? stat(bytes, &result->status)
        : lstat(bytes, &result->status);

    if (status != 0){
        LT_system_error("Could not stat pathname", errno);
    }
    result->pathname = path;
    return result;
}

LT_DEFINE_PRIMITIVE(pathname_stat_class_method_file,
                    "PathnameStat class>>file:", "(self path)",
                    "Return POSIX status for a String or Pathname."){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_Value path;
    (void)tail_call_unwind_marker;
    LT_OBJECT_ARG(cursor, self);
    LT_OBJECT_ARG(cursor, path);
    LT_ARG_END(cursor);
    if (self != LT_STATIC_CLASS(LT_PathnameStat)){
        LT_error("file: is only supported on PathnameStat");
    }
    return (LT_Value)(uintptr_t)PathnameStat_new_for_value(path, 1);
}

static LT_Method_Descriptor Pathname_methods[] = {
    {"asString", &pathname_method_as_string},
    {"absolute?", &pathname_method_absolute_p},
    {"relative?", &pathname_method_relative_p},
    {"/", &pathname_method_append},
    {"parent", &pathname_method_parent},
    {"exists?", &pathname_method_exists_p},
    {"isDirectory?", &pathname_method_directory_p},
    {"isRegularFile?", &pathname_method_regular_file_p},
    {"isSymbolicLink?", &pathname_method_symbolic_link_p},
    {"isFIFO?", &pathname_method_fifo_p},
    {"isSocket?", &pathname_method_socket_p},
    {"isCharacterDevice?", &pathname_method_character_device_p},
    {"isBlockDevice?", &pathname_method_block_device_p},
    {"readable?", &pathname_method_readable_p},
    {"writable?", &pathname_method_writable_p},
    {"executable?", &pathname_method_executable_p},
    {"stat", &pathname_method_stat},
    {"lstat", &pathname_method_lstat},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor Pathname_class_methods[] = {
    {"fromString:", &pathname_class_method_from_string},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor RelativePathname_class_methods[] = {
    {"fromString:", &relative_pathname_class_method_from_string},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor AbsolutePathname_class_methods[] = {
    {"fromString:", &absolute_pathname_class_method_from_string},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor AbsolutePathname_methods[] = {
    {"rootedAt:", &absolute_pathname_method_rooted_at},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor PathnameStat_methods[] = {
    {"pathname", &pathname_stat_method_pathname},
    {"isDirectory?", &pathname_stat_method_directory_p},
    {"isRegularFile?", &pathname_stat_method_regular_file_p},
    {"isSymbolicLink?", &pathname_stat_method_symbolic_link_p},
    {"isFIFO?", &pathname_stat_method_fifo_p},
    {"isSocket?", &pathname_stat_method_socket_p},
    {"isCharacterDevice?", &pathname_stat_method_character_device_p},
    {"isBlockDevice?", &pathname_stat_method_block_device_p},
    {"size", &pathname_stat_method_size},
    {"mode", &pathname_stat_method_mode},
    {"uid", &pathname_stat_method_uid},
    {"gid", &pathname_stat_method_gid},
    {"accessedAt", &pathname_stat_method_accessed_at},
    {"modifiedAt", &pathname_stat_method_modified_at},
    {"statusChangedAt", &pathname_stat_method_status_changed_at},
    {"regular-file?", &pathname_stat_method_regular_file_p},
    {"directory?", &pathname_stat_method_directory_p},
    {"pipe?", &pathname_stat_method_fifo_p},
    {"device?", &pathname_stat_method_device_p},
    {"socket?", &pathname_stat_method_socket_p},
    {"dev", &pathname_stat_method_dev},
    {"ino", &pathname_stat_method_ino},
    {"nlink", &pathname_stat_method_nlink},
    {"rdev", &pathname_stat_method_rdev},
    {"blksize", &pathname_stat_method_blksize},
    {"blocks", &pathname_stat_method_blocks},
    {"atime", &pathname_stat_method_accessed_at},
    {"mtime", &pathname_stat_method_modified_at},
    {"ctime", &pathname_stat_method_status_changed_at},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor PathnameStat_class_methods[] = {
    {"file:", &pathname_stat_class_method_file},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Slot_Descriptor PathnameStat_slots[] = {
    {"path", offsetof(LT_PathnameStat, pathname), &LT_SlotType_ReadonlyObject},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_Pathname) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Pathname",
    .documentation = "Abstract normalized UTF-8 filesystem pathname.",
    .instance_size = sizeof(LT_Pathname),
    .class_flags = LT_CLASS_FLAG_ABSTRACT | LT_CLASS_FLAG_IMMUTABLE
        | LT_CLASS_FLAG_SCALAR,
    .hash = Pathname_hash,
    .equal_p = Pathname_equal_p,
    .debugPrintOn = Pathname_debugPrintOn,
    .methods = Pathname_methods,
    .class_methods = Pathname_class_methods,
};

LT_DEFINE_CLASS(LT_RelativePathname) {
    .superclass = &LT_Pathname_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "RelativePathname",
    .documentation = "Normalized relative filesystem pathname.",
    .instance_size = sizeof(LT_RelativePathname),
    .class_flags = LT_CLASS_FLAG_FINAL | LT_CLASS_FLAG_IMMUTABLE
        | LT_CLASS_FLAG_SCALAR,
    .debugPrintOn = Pathname_debugPrintOn,
    .class_methods = RelativePathname_class_methods,
};

LT_DEFINE_CLASS(LT_AbsolutePathname) {
    .superclass = &LT_Pathname_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "AbsolutePathname",
    .documentation = "Normalized absolute filesystem pathname.",
    .instance_size = sizeof(LT_AbsolutePathname),
    .class_flags = LT_CLASS_FLAG_FINAL | LT_CLASS_FLAG_IMMUTABLE
        | LT_CLASS_FLAG_SCALAR,
    .debugPrintOn = Pathname_debugPrintOn,
    .methods = AbsolutePathname_methods,
    .class_methods = AbsolutePathname_class_methods,
};

LT_DEFINE_CLASS(LT_PathnameStat) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "PathnameStat",
    .documentation = "Immutable snapshot of POSIX pathname metadata.",
    .instance_size = sizeof(LT_PathnameStat),
    .class_flags = LT_CLASS_FLAG_FINAL | LT_CLASS_FLAG_IMMUTABLE,
    .slots = PathnameStat_slots,
    .methods = PathnameStat_methods,
    .class_methods = PathnameStat_class_methods,
};

LT_Pathname* LT_Pathname_new(char* pathname){
    return pathname[0] == '/'
        ? (LT_Pathname*)LT_AbsolutePathname_new(pathname)
        : (LT_Pathname*)LT_RelativePathname_new(pathname);
}

LT_Pathname* LT_Pathname_from_string(LT_String* string){
    Pathname_check_string(string);
    return LT_String_value_cstr(string)[0] == '/'
        ? (LT_Pathname*)LT_AbsolutePathname_from_string(string)
        : (LT_Pathname*)LT_RelativePathname_from_string(string);
}

LT_RelativePathname* LT_RelativePathname_new(char* pathname){
    return LT_RelativePathname_from_string(LT_String_new_cstr(pathname));
}

LT_RelativePathname* LT_RelativePathname_from_string(LT_String* string){
    Pathname_check_string(string);
    if (LT_String_value_cstr(string)[0] == '/'){
        LT_error("Relative pathname cannot start with slash");
    }
    return (LT_RelativePathname*)Pathname_allocate(
        &LT_RelativePathname_class,
        Pathname_normalize(LT_String_value_cstr(string), 0)
    );
}

LT_AbsolutePathname* LT_AbsolutePathname_new(char* pathname){
    return LT_AbsolutePathname_from_string(LT_String_new_cstr(pathname));
}

LT_AbsolutePathname* LT_AbsolutePathname_from_string(LT_String* string){
    Pathname_check_string(string);
    return (LT_AbsolutePathname*)Pathname_allocate(
        &LT_AbsolutePathname_class,
        Pathname_normalize(LT_String_value_cstr(string), 1)
    );
}

LT_Pathname* LT_Pathname_append(LT_Pathname* left, LT_Pathname* right){
    const char* left_bytes = LT_Pathname_value_cstr(left);
    const char* right_bytes = LT_Pathname_value_cstr(right);
    const char* right_suffix;
    size_t left_length;
    size_t right_length;
    char* combined;

    if (LT_Pathname_absolute_p(right)){
        LT_error("Cannot append an absolute pathname");
    }
    right_suffix = strcmp(right_bytes, ".") == 0
        ? ""
        : (right_bytes[0] == '.' && right_bytes[1] == '/'
            ? right_bytes + 2
            : right_bytes);
    if (strcmp(left_bytes, ".") == 0){
        return LT_Pathname_new((char*)(*right_suffix == '\0' ? "." : right_suffix));
    }
    left_length = strlen(left_bytes);
    right_length = strlen(right_suffix);
    combined = GC_MALLOC_ATOMIC(left_length + right_length + 2);
    memcpy(combined, left_bytes, left_length);
    combined[left_length] = '/';
    memcpy(combined + left_length + 1, right_suffix, right_length + 1);
    return LT_Pathname_new(combined);
}

LT_Pathname* LT_Pathname_parent(LT_Pathname* pathname){
    const char* bytes = LT_Pathname_value_cstr(pathname);
    const char* slash;
    size_t length;
    char* parent;

    if (strcmp(bytes, ".") == 0 || strcmp(bytes, "/") == 0){
        LT_error("Pathname has no parent");
    }
    slash = strrchr(bytes, '/');
    if (slash == NULL){
        return LT_Pathname_new(".");
    }
    if (slash == bytes){
        return LT_Pathname_new("/");
    }
    length = (size_t)(slash - bytes);
    if (length == 1 && bytes[0] == '.'){
        return LT_Pathname_new(".");
    }
    parent = GC_MALLOC_ATOMIC(length + 1);
    memcpy(parent, bytes, length);
    parent[length] = '\0';
    return LT_Pathname_new(parent);
}

LT_Pathname* LT_AbsolutePathname_rooted_at(LT_AbsolutePathname* pathname,
                                           LT_Pathname* root){
    const char* suffix = LT_Pathname_value_cstr((LT_Pathname*)pathname) + 1;
    LT_RelativePathname* relative = LT_RelativePathname_new(
        (char*)(*suffix == '\0' ? "." : suffix)
    );

    return LT_Pathname_append(root, (LT_Pathname*)relative);
}

LT_String* LT_Pathname_as_string(LT_Pathname* pathname){
    return LT_String_new_cstr(pathname->pathname);
}

const char* LT_Pathname_value_cstr(LT_Pathname* pathname){
    return pathname->pathname;
}

int LT_Pathname_absolute_p(LT_Pathname* pathname){
    return LT_AbsolutePathname_p((LT_Value)(uintptr_t)pathname);
}

int LT_Pathname_relative_p(LT_Pathname* pathname){
    return LT_RelativePathname_p((LT_Value)(uintptr_t)pathname);
}

int LT_Pathname_exists_p(LT_Pathname* pathname){
    struct stat status;

    return Pathname_stat_buffer(pathname, &status, 1, 1);
}

static int Pathname_mode_p(LT_Pathname* pathname,
                           int follow_links,
                           int (*predicate)(mode_t)){
    struct stat status;

    return Pathname_stat_buffer(pathname, &status, follow_links, 1)
        && predicate(status.st_mode);
}

static int Pathname_mode_directory(mode_t mode){ return S_ISDIR(mode); }
static int Pathname_mode_regular(mode_t mode){ return S_ISREG(mode); }
static int Pathname_mode_link(mode_t mode){ return S_ISLNK(mode); }
static int Pathname_mode_fifo(mode_t mode){ return S_ISFIFO(mode); }
static int Pathname_mode_socket(mode_t mode){ return S_ISSOCK(mode); }
static int Pathname_mode_character(mode_t mode){ return S_ISCHR(mode); }
static int Pathname_mode_block(mode_t mode){ return S_ISBLK(mode); }

int LT_Pathname_directory_p(LT_Pathname* pathname){
    return Pathname_mode_p(pathname, 1, Pathname_mode_directory);
}

int LT_Pathname_regular_file_p(LT_Pathname* pathname){
    return Pathname_mode_p(pathname, 1, Pathname_mode_regular);
}

int LT_Pathname_symbolic_link_p(LT_Pathname* pathname){
    return Pathname_mode_p(pathname, 0, Pathname_mode_link);
}

int LT_Pathname_fifo_p(LT_Pathname* pathname){
    return Pathname_mode_p(pathname, 1, Pathname_mode_fifo);
}

int LT_Pathname_socket_p(LT_Pathname* pathname){
    return Pathname_mode_p(pathname, 1, Pathname_mode_socket);
}

int LT_Pathname_character_device_p(LT_Pathname* pathname){
    return Pathname_mode_p(pathname, 1, Pathname_mode_character);
}

int LT_Pathname_block_device_p(LT_Pathname* pathname){
    return Pathname_mode_p(pathname, 1, Pathname_mode_block);
}

int LT_Pathname_readable_p(LT_Pathname* pathname){
    return access(LT_Pathname_value_cstr(pathname), R_OK) == 0;
}

int LT_Pathname_writable_p(LT_Pathname* pathname){
    return access(LT_Pathname_value_cstr(pathname), W_OK) == 0;
}

int LT_Pathname_executable_p(LT_Pathname* pathname){
    return access(LT_Pathname_value_cstr(pathname), X_OK) == 0;
}

LT_PathnameStat* LT_Pathname_stat(LT_Pathname* pathname){
    return PathnameStat_new_for_value((LT_Value)(uintptr_t)pathname, 1);
}

LT_PathnameStat* LT_Pathname_lstat(LT_Pathname* pathname){
    return PathnameStat_new_for_value((LT_Value)(uintptr_t)pathname, 0);
}

const char* LT_Pathname_like_value_cstr(LT_Value value){
    if (LT_Pathname_p(value)){
        return LT_Pathname_value_cstr(LT_Pathname_from_value(value));
    }
    if (LT_String_p(value)){
        LT_String* string = LT_String_from_value(value);
        Pathname_check_string(string);
        return LT_String_value_cstr(string);
    }
    LT_error("Expected Pathname or String");
    return NULL;
}

LT_String* LT_Pathname_like_as_string(LT_Value value){
    if (LT_Pathname_p(value)){
        return LT_Pathname_as_string(LT_Pathname_from_value(value));
    }
    (void)LT_Pathname_like_value_cstr(value);
    return LT_String_from_value(value);
}
