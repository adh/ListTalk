/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__RandomAccessFile__
#define H__ListTalk__RandomAccessFile__

#include <ListTalk/classes/String.h>
#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_RandomAccessFile);

LT_RandomAccessFile* LT_RandomAccessFile_forReading(LT_String* filename);
LT_RandomAccessFile* LT_RandomAccessFile_forWriting(LT_String* filename);

LT__END_DECLS

#endif
