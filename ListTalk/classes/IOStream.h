/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__IOStream__
#define H__ListTalk__IOStream__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

#include <stdio.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_IOStream);
LT_DECLARE_CLASS(LT_InputStream);
LT_DECLARE_CLASS(LT_OutputStream);
LT_DECLARE_CLASS(LT_FileInputStream);
LT_DECLARE_CLASS(LT_FileOutputStream);
LT_DECLARE_CLASS(LT_FileIOStream);

LT_FileInputStream* LT_FileInputStream_newForFilename(char* filename);
LT_FileInputStream* LT_FileInputStream_newForFILE(FILE* file, int owns_file);
LT_FileInputStream* LT_FileInputStream_newForOwnedFILE(FILE* file);
LT_FileInputStream* LT_FileInputStream_newForBorrowedFILE(FILE* file);

LT_FileOutputStream* LT_FileOutputStream_newForFilename(char* filename);
LT_FileOutputStream* LT_FileOutputStream_newForFILE(FILE* file, int owns_file);
LT_FileOutputStream* LT_FileOutputStream_newForOwnedFILE(FILE* file);
LT_FileOutputStream* LT_FileOutputStream_newForBorrowedFILE(FILE* file);

LT_FileIOStream* LT_FileIOStream_newForFilename(char* filename);
LT_FileIOStream* LT_FileIOStream_newForFILE(FILE* file, int owns_file);
LT_FileIOStream* LT_FileIOStream_newForOwnedFILE(FILE* file);
LT_FileIOStream* LT_FileIOStream_newForBorrowedFILE(FILE* file);

int LT_IOStream_closed(LT_IOStream* stream);
void LT_IOStream_close(LT_IOStream* stream);
void LT_IOStream_flush(LT_IOStream* stream);
void LT_IOStream_seek(LT_IOStream* stream, long offset);
void LT_IOStream_seekFromEnd(LT_IOStream* stream, long offset);
void LT_IOStream_seekFromStart(LT_IOStream* stream, long offset);

void LT_OutputStream_writeByte(LT_IOStream* stream, uint8_t byte);
void LT_OutputStream_writeCharacter(LT_IOStream* stream, uint32_t codepoint);
void LT_OutputStream_writeByteVector(LT_IOStream* stream,
                                     LT_ByteVector* bytevector);
void LT_OutputStream_writeString(LT_IOStream* stream, LT_String* string);
void LT_OutputStream_writeLn(LT_IOStream* stream);

LT_Value LT_InputStream_readByte(LT_IOStream* stream);
LT_ByteVector* LT_InputStream_readBytes(LT_IOStream* stream, size_t length);
LT_Value LT_InputStream_readLine(LT_IOStream* stream);
LT_String* LT_InputStream_readString(LT_IOStream* stream);
LT_ByteVector* LT_InputStream_readByteVector(LT_IOStream* stream);

LT__END_DECLS

#endif
