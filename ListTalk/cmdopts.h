/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__cmdopts__
#define H__ListTalk__cmdopts__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/vm/value.h>

LT__BEGIN_DECLS

typedef struct LT_CmdOpts LT_CmdOpts;

typedef void (*LT_CmdOptsCallback)(
    LT_CmdOpts* parser,
    void* baton,
    char* value
);

#define LT_CMDOPTS_STRICT_ORDER 1

LT_CmdOpts* LT_CmdOpts_new(int flags);
void LT_CmdOpts_addOption(
    LT_CmdOpts* parser,
    int has_argument,
    char short_option,
    char* long_option,
    LT_CmdOptsCallback callback,
    void* baton
);

#define LT_CMDOPTS_ARGUMENT_REQUIRED 1
#define LT_CMDOPTS_ARGUMENT_MULTIPLE 2

void LT_CmdOpts_addArgument(
    LT_CmdOpts* parser,
    int flags,
    LT_CmdOptsCallback callback,
    void* baton
);

typedef char* (*LT_CmdOptsSource)(void* baton);

void LT_CmdOpts_parse(
    LT_CmdOpts* parser,
    LT_CmdOptsSource source,
    void* baton
);
void LT_CmdOpts_parseArgv(LT_CmdOpts* parser, int argc, char** argv);
void LT_CmdOpts_parseList(LT_CmdOpts* parser, LT_Value list);
LT_Value LT_CmdOpts_argvToList(int argc, char** argv);

void LT_CmdOpts_addStringArgument(
    LT_CmdOpts* parser,
    int required,
    char** value
);
void LT_CmdOpts_addStringListArgument(
    LT_CmdOpts* parser,
    int required,
    LT_Value* value
);
void LT_CmdOpts_addStringOption(
    LT_CmdOpts* parser,
    char short_option,
    char* long_option,
    char** value
);
void LT_CmdOpts_addLongArgument(
    LT_CmdOpts* parser,
    int required,
    long* value
);
void LT_CmdOpts_addLongOption(
    LT_CmdOpts* parser,
    char short_option,
    char* long_option,
    long* value
);
void LT_CmdOpts_addDoubleArgument(
    LT_CmdOpts* parser,
    int required,
    double* value
);
void LT_CmdOpts_addDoubleOption(
    LT_CmdOpts* parser,
    char short_option,
    char* long_option,
    double* value
);
void LT_CmdOpts_addFlagSet(
    LT_CmdOpts* parser,
    char short_option,
    char* long_option,
    int value,
    int* place
);
void LT_CmdOpts_addFlagIncrement(
    LT_CmdOpts* parser,
    char short_option,
    char* long_option,
    int value,
    int* place
);

LT__END_DECLS

#endif
