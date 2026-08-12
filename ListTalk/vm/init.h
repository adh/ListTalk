/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__vm__init__
#define H__ListTalk__vm__init__
#include <ListTalk/macros/env_macros.h>

LT__BEGIN_DECLS

extern void LT__init(void);

#define LT_INIT()   \
    do {            \
        GC_INIT();  \
        LT__init(); \
    } while (0)


LT__END_DECLS

#endif