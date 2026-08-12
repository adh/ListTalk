/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__vm__signal__
#define H__ListTalk__vm__signal__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/vm/value.h>

LT__BEGIN_DECLS

extern void LT_register_posix_signal(int signal_number, LT_Value callable);
extern void LT_unregister_posix_signal(int signal_number);
extern void LT_enable_KeyboardInterrupt(void);
extern void LT_disable_KeyboardInterrupt(void);

LT__END_DECLS

#endif