/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__ReadWriteLock__
#define H__ListTalk__ReadWriteLock__

#include <ListTalk/macros/env_macros.h>
#include <ListTalk/macros/decl_macros.h>
#include <ListTalk/utils/lock.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_ReadWriteLock);

LT_ReadWriteLock* LT_ReadWriteLock_new(void);
LT_ReadWriteLock* LT_ReadWriteLock_new_named(LT_Value name);
void LT_ReadWriteLock_readLock(LT_ReadWriteLock* lock);
int LT_ReadWriteLock_tryReadLock(LT_ReadWriteLock* lock);
void LT_ReadWriteLock_readUnlock(LT_ReadWriteLock* lock);
void LT_ReadWriteLock_writeLock(LT_ReadWriteLock* lock);
int LT_ReadWriteLock_tryWriteLock(LT_ReadWriteLock* lock);
void LT_ReadWriteLock_writeUnlock(LT_ReadWriteLock* lock);
LT_Value LT_ReadWriteLock_name(LT_ReadWriteLock* lock);

LT__END_DECLS

#endif
