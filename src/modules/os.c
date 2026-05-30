/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Integer.h>
#include <ListTalk/classes/Instant.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Package.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/loader.h>

#include <dirent.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>

LT_DECLARE_CLASS(LT_OS_Stat);

struct LT_OS_Stat_s {
    LT_Object base;
    LT_Value path;
    struct stat stat_buffer;
};

static mode_t mode_from_fixnum(int64_t value){
    if (value < 0){
        LT_error("Negative mode");
    }
    return (mode_t)value;
}

static int path_is_directory(const char* path){
    struct stat stat_buffer;

    return stat(path, &stat_buffer) == 0 && S_ISDIR(stat_buffer.st_mode);
}

static void mkdir_existing_ok(const char* path, mode_t mode){
    if (mkdir(path, mode) == 0){
        return;
    }
    if (errno == EEXIST && path_is_directory(path)){
        return;
    }
    LT_system_error("Could not create directory", errno);
}

static void mkdirs(const char* path, mode_t mode){
    size_t length = strlen(path);
    char* copy;
    char* cursor;

    if (length == 0){
        mkdir_existing_ok(path, mode);
        return;
    }

    copy = GC_MALLOC_ATOMIC(length + 1);
    memcpy(copy, path, length + 1);

    cursor = copy;
    while (*cursor == '/'){
        cursor++;
    }

    while (*cursor != '\0'){
        if (*cursor == '/'){
            *cursor = '\0';
            mkdir_existing_ok(copy, mode);
            *cursor = '/';
            while (cursor[1] == '/'){
                cursor++;
            }
        }
        cursor++;
    }

    mkdir_existing_ok(copy, mode);
}

static LT_OS_Stat* os_stat_new(LT_String* path){
    LT_OS_Stat* stat_object = LT_Class_ALLOC(LT_OS_Stat);

    stat_object->path = (LT_Value)(uintptr_t)path;
    if (stat(LT_String_value_cstr(path), &stat_object->stat_buffer) != 0){
        LT_system_error("Could not stat path", errno);
    }
    return stat_object;
}

static int mode_is_regular_file(mode_t mode){
    return S_ISREG(mode);
}

static int mode_is_directory(mode_t mode){
    return S_ISDIR(mode);
}

static int mode_is_pipe(mode_t mode){
    return S_ISFIFO(mode);
}

static int mode_is_device(mode_t mode){
    return S_ISCHR(mode) || S_ISBLK(mode);
}

static int mode_is_socket(mode_t mode){
#ifdef S_ISSOCK
    return S_ISSOCK(mode);
#else
    (void)mode;
    return 0;
#endif
}

static LT_Value boolean_from_int(int value){
    return value ? LT_TRUE : LT_FALSE;
}

static LT_Value instant_from_epoch_seconds(intmax_t seconds){
    return LT_Instant_new(LT_Number_multiply2(
        LT_Integer_from_intmax(seconds),
        LT_SmallInteger_new(1000000)
    ));
}

static LT_Value stat_predicate_from_path(LT_String* path, int (*predicate)(mode_t)){
    struct stat stat_buffer;

    if (stat(LT_String_value_cstr(path), &stat_buffer) != 0){
        LT_system_error("Could not stat path", errno);
    }
    return boolean_from_int(predicate(stat_buffer.st_mode));
}

static LT_String* string_from_getcwd(void){
    size_t size = 256;

    while (1){
        char* buffer = GC_MALLOC_ATOMIC(size);

        errno = 0;
        if (getcwd(buffer, size) != NULL){
            return LT_String_new_cstr(buffer);
        }
        if (errno != ERANGE){
            LT_system_error("Could not get current working directory", errno);
        }
        size *= 2;
    }
}

static void bind_os_primitive(LT_Environment* environment,
                              LT_Package* package,
                              LT_Primitive* primitive){
    LT_Environment_bind(
        environment,
        LT_Symbol_new_in(package, primitive->name),
        LT_Primitive_from_static(primitive),
        LT_ENV_BINDING_FLAG_CONSTANT
    );
}

#define OS_STRING_ARG(cursor, name) \
    LT_GENERIC_ARG(cursor, name, LT_String*, LT_String_from_value)

#define OS_STAT_ARG(cursor, name) \
    LT_GENERIC_ARG(cursor, name, LT_OS_Stat*, LT_OS_Stat_from_value)

#define DEFINE_OS_STAT_PREDICATE_METHOD(c_name, selector, predicate, description) \
    LT_DEFINE_PRIMITIVE(                                                          \
        c_name,                                                                   \
        "Stat>>" selector,                                                        \
        "(self)",                                                                 \
        description                                                               \
    ){                                                                            \
        LT_Value cursor = arguments;                                              \
        LT_OS_Stat* self;                                                         \
        (void)tail_call_unwind_marker;                                            \
        (void)invocation_context_kind;                                            \
        (void)invocation_context_data;                                            \
                                                                                  \
        OS_STAT_ARG(cursor, self);                                                \
        LT_ARG_END(cursor);                                                       \
                                                                                  \
        return boolean_from_int(predicate(self->stat_buffer.st_mode));            \
    }

#define DEFINE_OS_PATH_PREDICATE(c_name, primitive_name, predicate, description) \
    LT_DEFINE_PRIMITIVE(                                                         \
        c_name,                                                                  \
        primitive_name,                                                           \
        "(path)",                                                                \
        description                                                              \
    ){                                                                           \
        LT_Value cursor = arguments;                                             \
        LT_String* path;                                                         \
        (void)tail_call_unwind_marker;                                           \
        (void)invocation_context_kind;                                           \
        (void)invocation_context_data;                                           \
                                                                                 \
        OS_STRING_ARG(cursor, path);                                             \
        LT_ARG_END(cursor);                                                      \
                                                                                 \
        return stat_predicate_from_path(path, predicate);                        \
    }

#define DEFINE_OS_STAT_UINT_FIELD_METHOD(c_name, selector, field, description) \
    LT_DEFINE_PRIMITIVE(                                                       \
        c_name,                                                                \
        "Stat>>" selector,                                                     \
        "(self)",                                                              \
        description                                                            \
    ){                                                                         \
        LT_Value cursor = arguments;                                           \
        LT_OS_Stat* self;                                                      \
        (void)tail_call_unwind_marker;                                         \
        (void)invocation_context_kind;                                         \
        (void)invocation_context_data;                                         \
                                                                               \
        OS_STAT_ARG(cursor, self);                                             \
        LT_ARG_END(cursor);                                                    \
                                                                               \
        return LT_Integer_from_uintmax((uintmax_t)self->stat_buffer.field);    \
    }

#define DEFINE_OS_STAT_INT_FIELD_METHOD(c_name, selector, field, description) \
    LT_DEFINE_PRIMITIVE(                                                      \
        c_name,                                                               \
        "Stat>>" selector,                                                    \
        "(self)",                                                             \
        description                                                           \
    ){                                                                        \
        LT_Value cursor = arguments;                                          \
        LT_OS_Stat* self;                                                     \
        (void)tail_call_unwind_marker;                                        \
        (void)invocation_context_kind;                                        \
        (void)invocation_context_data;                                        \
                                                                              \
        OS_STAT_ARG(cursor, self);                                            \
        LT_ARG_END(cursor);                                                   \
                                                                              \
        return LT_Integer_from_intmax((intmax_t)self->stat_buffer.field);     \
    }

#define DEFINE_OS_STAT_TIME_FIELD_METHOD(c_name, selector, field, description) \
    LT_DEFINE_PRIMITIVE(                                                       \
        c_name,                                                                \
        "Stat>>" selector,                                                     \
        "(self)",                                                              \
        description                                                            \
    ){                                                                         \
        LT_Value cursor = arguments;                                           \
        LT_OS_Stat* self;                                                      \
        (void)tail_call_unwind_marker;                                         \
        (void)invocation_context_kind;                                         \
        (void)invocation_context_data;                                         \
                                                                               \
        OS_STAT_ARG(cursor, self);                                             \
        LT_ARG_END(cursor);                                                    \
                                                                               \
        return instant_from_epoch_seconds((intmax_t)self->stat_buffer.field);  \
    }

LT_DEFINE_PRIMITIVE(
    stat_class_method_file,
    "Stat class>>file:",
    "(self path)",
    "Return filesystem status for path."
){
    LT_Value cursor = arguments;
    LT_Value self;
    LT_String* path;
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_OBJECT_ARG(cursor, self);
    OS_STRING_ARG(cursor, path);
    LT_ARG_END(cursor);
    (void)self;

    return (LT_Value)(uintptr_t)os_stat_new(path);
}

DEFINE_OS_STAT_PREDICATE_METHOD(
    stat_method_regular_file_p,
    "regular-file?",
    mode_is_regular_file,
    "Return true when this status describes a regular file."
)

DEFINE_OS_STAT_PREDICATE_METHOD(
    stat_method_directory_p,
    "directory?",
    mode_is_directory,
    "Return true when this status describes a directory."
)

DEFINE_OS_STAT_PREDICATE_METHOD(
    stat_method_pipe_p,
    "pipe?",
    mode_is_pipe,
    "Return true when this status describes a pipe."
)

DEFINE_OS_STAT_PREDICATE_METHOD(
    stat_method_device_p,
    "device?",
    mode_is_device,
    "Return true when this status describes a device."
)

DEFINE_OS_STAT_PREDICATE_METHOD(
    stat_method_socket_p,
    "socket?",
    mode_is_socket,
    "Return true when this status describes a socket."
)

DEFINE_OS_STAT_UINT_FIELD_METHOD(
    stat_method_dev,
    "dev",
    st_dev,
    "Return the device ID."
)

DEFINE_OS_STAT_UINT_FIELD_METHOD(
    stat_method_ino,
    "ino",
    st_ino,
    "Return the inode number."
)

DEFINE_OS_STAT_UINT_FIELD_METHOD(
    stat_method_mode,
    "mode",
    st_mode,
    "Return the file mode bits."
)

DEFINE_OS_STAT_UINT_FIELD_METHOD(
    stat_method_nlink,
    "nlink",
    st_nlink,
    "Return the link count."
)

DEFINE_OS_STAT_UINT_FIELD_METHOD(
    stat_method_uid,
    "uid",
    st_uid,
    "Return the owner user ID."
)

DEFINE_OS_STAT_UINT_FIELD_METHOD(
    stat_method_gid,
    "gid",
    st_gid,
    "Return the owner group ID."
)

DEFINE_OS_STAT_UINT_FIELD_METHOD(
    stat_method_rdev,
    "rdev",
    st_rdev,
    "Return the device ID for special files."
)

DEFINE_OS_STAT_INT_FIELD_METHOD(
    stat_method_size,
    "size",
    st_size,
    "Return the file size in bytes."
)

DEFINE_OS_STAT_INT_FIELD_METHOD(
    stat_method_blksize,
    "blksize",
    st_blksize,
    "Return the preferred block size for filesystem I/O."
)

DEFINE_OS_STAT_INT_FIELD_METHOD(
    stat_method_blocks,
    "blocks",
    st_blocks,
    "Return the number of allocated blocks."
)

DEFINE_OS_STAT_TIME_FIELD_METHOD(
    stat_method_atime,
    "atime",
    st_atime,
    "Return the last access time as an Instant."
)

DEFINE_OS_STAT_TIME_FIELD_METHOD(
    stat_method_mtime,
    "mtime",
    st_mtime,
    "Return the last modification time as an Instant."
)

DEFINE_OS_STAT_TIME_FIELD_METHOD(
    stat_method_ctime,
    "ctime",
    st_ctime,
    "Return the last status change time as an Instant."
)

static LT_Slot_Descriptor Stat_slots[] = {
    {"path", offsetof(LT_OS_Stat, path), &LT_SlotType_ReadonlyObject},
    LT_NULL_NATIVE_CLASS_SLOT_DESCRIPTOR
};

static LT_Method_Descriptor Stat_methods[] = {
    {"regular-file?", &stat_method_regular_file_p},
    {"directory?", &stat_method_directory_p},
    {"pipe?", &stat_method_pipe_p},
    {"device?", &stat_method_device_p},
    {"socket?", &stat_method_socket_p},
    {"dev", &stat_method_dev},
    {"ino", &stat_method_ino},
    {"mode", &stat_method_mode},
    {"nlink", &stat_method_nlink},
    {"uid", &stat_method_uid},
    {"gid", &stat_method_gid},
    {"rdev", &stat_method_rdev},
    {"size", &stat_method_size},
    {"blksize", &stat_method_blksize},
    {"blocks", &stat_method_blocks},
    {"atime", &stat_method_atime},
    {"mtime", &stat_method_mtime},
    {"ctime", &stat_method_ctime},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

static LT_Method_Descriptor Stat_class_methods[] = {
    {"file:", &stat_class_method_file},
    LT_NULL_NATIVE_CLASS_METHOD_DESCRIPTOR
};

LT_DEFINE_CLASS(LT_OS_Stat) {
    .superclass = &LT_Object_class,
    .metaclass_superclass = &LT_Class_class,
    .name = "Stat",
    .documentation = "Filesystem metadata returned by stat operations.",
    .instance_size = sizeof(LT_OS_Stat),
    .class_flags = LT_CLASS_FLAG_FINAL,
    .slots = Stat_slots,
    .methods = Stat_methods,
    .class_methods = Stat_class_methods,
};

LT_DEFINE_PRIMITIVE(
    primitive_os_exit,
    "exit",
    "(:optional status)",
    "Terminate process with optional fixnum status code."
){
    LT_Value cursor = arguments;
    LT_Value status = LT_SmallInteger_new(0);
    (void)tail_call_unwind_marker;
    (void)invocation_context_kind;
    (void)invocation_context_data;

    LT_OBJECT_ARG_OPT(cursor, status, status);
    LT_ARG_END(cursor);

    exit(LT_Number_int_from_integer(
        status,
        INT_MIN,
        INT_MAX,
        "Exit status out of range"
    ));
    return LT_NIL;
}

LT_DEFINE_PRIMITIVE(
    primitive_os_getenv,
    "getenv",
    "(name)",
    "Return environment variable value as string, or nil when missing."
){
    LT_Value cursor = arguments;
    LT_String* name;
    const char* value;

    OS_STRING_ARG(cursor, name);
    LT_ARG_END(cursor);

    value = getenv(LT_String_value_cstr(name));
    if (value == NULL){
        return LT_NIL;
    }
    return (LT_Value)(uintptr_t)LT_String_new_cstr((char*)value);
}

LT_DEFINE_PRIMITIVE(
    primitive_os_getpwd,
    "getpwd",
    "()",
    "Return the current working directory."
){
    LT_ARG_END(arguments);
    return (LT_Value)(uintptr_t)string_from_getcwd();
}

LT_DEFINE_PRIMITIVE(
    primitive_os_chdir,
    "chdir",
    "(path)",
    "Change the current working directory and return the new path."
){
    LT_Value cursor = arguments;
    LT_String* path;

    OS_STRING_ARG(cursor, path);
    LT_ARG_END(cursor);

    if (chdir(LT_String_value_cstr(path)) != 0){
        LT_system_error("Could not change directory", errno);
    }
    return (LT_Value)(uintptr_t)string_from_getcwd();
}

LT_DEFINE_PRIMITIVE(
    primitive_os_access,
    "access",
    "(path :optional mode)",
    "Return true when path is accessible with the requested POSIX mode."
){
    LT_Value cursor = arguments;
    LT_String* path;
    LT_Value mode_value = LT_SmallInteger_new(F_OK);

    OS_STRING_ARG(cursor, path);
    LT_OBJECT_ARG_OPT(cursor, mode_value, mode_value);
    LT_ARG_END(cursor);

    return access(
        LT_String_value_cstr(path),
        LT_Number_int_from_integer(
            mode_value,
            INT_MIN,
            INT_MAX,
            "Access mode out of range"
        )
    ) == 0
        ? LT_TRUE
        : LT_FALSE;
}

DEFINE_OS_PATH_PREDICATE(
    primitive_os_regular_file_p,
    "regular-file?",
    mode_is_regular_file,
    "Return true when path names a regular file."
)

DEFINE_OS_PATH_PREDICATE(
    primitive_os_directory_p,
    "directory?",
    mode_is_directory,
    "Return true when path names a directory."
)

DEFINE_OS_PATH_PREDICATE(
    primitive_os_pipe_p,
    "pipe?",
    mode_is_pipe,
    "Return true when path names a pipe."
)

DEFINE_OS_PATH_PREDICATE(
    primitive_os_device_p,
    "device?",
    mode_is_device,
    "Return true when path names a device."
)

DEFINE_OS_PATH_PREDICATE(
    primitive_os_socket_p,
    "socket?",
    mode_is_socket,
    "Return true when path names a socket."
)

LT_DEFINE_PRIMITIVE(
    primitive_os_mkdir,
    "mkdir",
    "(path :optional mode)",
    "Create a directory and return its path."
){
    LT_Value cursor = arguments;
    LT_String* path;
    LT_Value mode_value = LT_SmallInteger_new(0777);
    int64_t mode;

    OS_STRING_ARG(cursor, path);
    LT_OBJECT_ARG_OPT(cursor, mode_value, mode_value);
    LT_ARG_END(cursor);

    mode = LT_SmallInteger_value(mode_value);
    if (mkdir(LT_String_value_cstr(path), mode_from_fixnum(mode)) != 0){
        LT_system_error("Could not create directory", errno);
    }
    return (LT_Value)(uintptr_t)path;
}

LT_DEFINE_PRIMITIVE(
    primitive_os_mkdirs,
    "mkdirs",
    "(path :optional mode)",
    "Create a directory and missing parent directories, returning its path."
){
    LT_Value cursor = arguments;
    LT_String* path;
    LT_Value mode_value = LT_SmallInteger_new(0777);
    int64_t mode;

    OS_STRING_ARG(cursor, path);
    LT_OBJECT_ARG_OPT(cursor, mode_value, mode_value);
    LT_ARG_END(cursor);

    mode = LT_SmallInteger_value(mode_value);
    mkdirs(LT_String_value_cstr(path), mode_from_fixnum(mode));
    return (LT_Value)(uintptr_t)path;
}

LT_DEFINE_PRIMITIVE(
    primitive_os_move,
    "move",
    "(old-path new-path)",
    "Rename or move a filesystem entry."
){
    LT_Value cursor = arguments;
    LT_String* old_path;
    LT_String* new_path;

    OS_STRING_ARG(cursor, old_path);
    OS_STRING_ARG(cursor, new_path);
    LT_ARG_END(cursor);

    if (rename(LT_String_value_cstr(old_path), LT_String_value_cstr(new_path)) != 0){
        LT_system_error("Could not move path", errno);
    }
    return (LT_Value)(uintptr_t)new_path;
}

LT_DEFINE_PRIMITIVE(
    primitive_os_unlink,
    "unlink",
    "(path)",
    "Remove a directory entry."
){
    LT_Value cursor = arguments;
    LT_String* path;

    OS_STRING_ARG(cursor, path);
    LT_ARG_END(cursor);

    if (unlink(LT_String_value_cstr(path)) != 0){
        LT_system_error("Could not unlink path", errno);
    }
    return LT_NIL;
}

LT_DEFINE_PRIMITIVE(
    primitive_os_link,
    "link",
    "(existing-path new-path)",
    "Create a hard link."
){
    LT_Value cursor = arguments;
    LT_String* existing_path;
    LT_String* new_path;

    OS_STRING_ARG(cursor, existing_path);
    OS_STRING_ARG(cursor, new_path);
    LT_ARG_END(cursor);

    if (link(LT_String_value_cstr(existing_path), LT_String_value_cstr(new_path)) != 0){
        LT_system_error("Could not create hard link", errno);
    }
    return (LT_Value)(uintptr_t)new_path;
}

LT_DEFINE_PRIMITIVE(
    primitive_os_readlink,
    "readlink",
    "(path)",
    "Return the symlink target path."
){
    LT_Value cursor = arguments;
    LT_String* path;
    size_t buffer_size = 256;

    OS_STRING_ARG(cursor, path);
    LT_ARG_END(cursor);

    while (1){
        char* buffer = GC_MALLOC_ATOMIC(buffer_size);
        ssize_t length = readlink(LT_String_value_cstr(path), buffer, buffer_size);

        if (length < 0){
            LT_system_error("Could not read symbolic link", errno);
        }
        if ((size_t)length < buffer_size){
            return (LT_Value)(uintptr_t)LT_String_new(buffer, (size_t)length);
        }
        buffer_size *= 2;
    }
}

LT_DEFINE_PRIMITIVE(
    primitive_os_symlink,
    "symlink",
    "(target-path link-path)",
    "Create a symbolic link."
){
    LT_Value cursor = arguments;
    LT_String* target_path;
    LT_String* link_path;

    OS_STRING_ARG(cursor, target_path);
    OS_STRING_ARG(cursor, link_path);
    LT_ARG_END(cursor);

    if (symlink(LT_String_value_cstr(target_path), LT_String_value_cstr(link_path)) != 0){
        LT_system_error("Could not create symbolic link", errno);
    }
    return (LT_Value)(uintptr_t)link_path;
}

LT_DEFINE_PRIMITIVE(
    primitive_os_getentropy,
    "getentropy",
    "(length)",
    "Return a bytevector with cryptographically strong random bytes."
){
    LT_Value cursor = arguments;
    int64_t length_value;
    size_t length;
    uint8_t* bytes;
    size_t offset = 0;

    LT_FIXNUM_ARG(cursor, length_value);
    LT_ARG_END(cursor);

    length = LT_Number_nonnegative_size_from_int64(
        length_value,
        "Negative length",
        "Length out of supported range"
    );
    bytes = GC_MALLOC_ATOMIC(length == 0 ? 1 : length);

    while (offset < length){
        size_t chunk = length - offset;

        if (chunk > 256){
            chunk = 256;
        }
        if (getentropy(bytes + offset, chunk) != 0){
            LT_system_error("Could not get entropy", errno);
        }
        offset += chunk;
    }
    return (LT_Value)(uintptr_t)LT_ByteVector_new(bytes, length);
}

LT_DEFINE_PRIMITIVE(
    primitive_os_list_directory,
    "list-directory",
    "(path)",
    "Return a list of filenames in a directory."
){
    LT_Value cursor = arguments;
    LT_String* path;
    DIR* directory;
    struct dirent* entry;
    LT_ListBuilder* builder;

    OS_STRING_ARG(cursor, path);
    LT_ARG_END(cursor);

    directory = opendir(LT_String_value_cstr(path));
    if (directory == NULL){
        LT_system_error("Could not open directory", errno);
    }

    builder = LT_ListBuilder_new();
    errno = 0;
    while ((entry = readdir(directory)) != NULL){
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
            errno = 0;
            continue;
        }
        LT_ListBuilder_append(
            builder,
            (LT_Value)(uintptr_t)LT_String_new_cstr(entry->d_name)
        );
        errno = 0;
    }

    if (errno != 0){
        int errnum = errno;

        closedir(directory);
        LT_system_error("Could not read directory", errnum);
    }
    if (closedir(directory) != 0){
        LT_system_error("Could not close directory", errno);
    }
    return LT_ListBuilder_value(builder);
}

void ListTalk_os_load(LT_Environment* environment){
    LT_Package* package = LT_Package_new("ListTalk-OS");

    LT_init_native_class(&LT_OS_Stat_class);
    LT_Environment_bind(
        environment,
        LT_Symbol_new_in(package, "Stat"),
        LT_STATIC_CLASS(LT_OS_Stat),
        LT_ENV_BINDING_FLAG_CONSTANT
    );

    bind_os_primitive(environment, package, &primitive_os_exit);
    bind_os_primitive(environment, package, &primitive_os_getenv);
    bind_os_primitive(environment, package, &primitive_os_getpwd);
    bind_os_primitive(environment, package, &primitive_os_chdir);
    bind_os_primitive(environment, package, &primitive_os_access);
    bind_os_primitive(environment, package, &primitive_os_regular_file_p);
    bind_os_primitive(environment, package, &primitive_os_directory_p);
    bind_os_primitive(environment, package, &primitive_os_pipe_p);
    bind_os_primitive(environment, package, &primitive_os_device_p);
    bind_os_primitive(environment, package, &primitive_os_socket_p);
    bind_os_primitive(environment, package, &primitive_os_mkdir);
    bind_os_primitive(environment, package, &primitive_os_mkdirs);
    bind_os_primitive(environment, package, &primitive_os_move);
    bind_os_primitive(environment, package, &primitive_os_unlink);
    bind_os_primitive(environment, package, &primitive_os_link);
    bind_os_primitive(environment, package, &primitive_os_readlink);
    bind_os_primitive(environment, package, &primitive_os_symlink);
    bind_os_primitive(environment, package, &primitive_os_getentropy);
    bind_os_primitive(environment, package, &primitive_os_list_directory);
    LT_loader_provide(environment, "os");
}
