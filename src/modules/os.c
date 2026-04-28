/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/classes/Number.h>
#include <ListTalk/classes/Package.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/classes/Symbol.h>
#include <ListTalk/utils.h>
#include <ListTalk/vm/error.h>
#include <ListTalk/vm/loader.h>

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>

static mode_t mode_from_fixnum(int64_t value){
    if (value < 0){
        LT_error("Negative mode");
    }
    return (mode_t)value;
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

LT_DEFINE_PRIMITIVE(
    primitive_os_exit,
    "exit",
    "([status])",
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
    "(path [mode])",
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

LT_DEFINE_PRIMITIVE(
    primitive_os_mkdir,
    "mkdir",
    "(path [mode])",
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

    bind_os_primitive(environment, package, &primitive_os_exit);
    bind_os_primitive(environment, package, &primitive_os_getenv);
    bind_os_primitive(environment, package, &primitive_os_getpwd);
    bind_os_primitive(environment, package, &primitive_os_chdir);
    bind_os_primitive(environment, package, &primitive_os_access);
    bind_os_primitive(environment, package, &primitive_os_mkdir);
    bind_os_primitive(environment, package, &primitive_os_move);
    bind_os_primitive(environment, package, &primitive_os_unlink);
    bind_os_primitive(environment, package, &primitive_os_link);
    bind_os_primitive(environment, package, &primitive_os_readlink);
    bind_os_primitive(environment, package, &primitive_os_symlink);
    bind_os_primitive(environment, package, &primitive_os_getentropy);
    bind_os_primitive(environment, package, &primitive_os_list_directory);
    LT_loader_provide(environment, "os");
}
