/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__Stream__
#define H__ListTalk__Stream__

#include <ListTalk/macros/env_macros.h>

#include <ListTalk/classes/ByteVector.h>
#include <ListTalk/classes/String.h>
#include <ListTalk/vm/value.h>
#include <ListTalk/macros/decl_macros.h>

#include <stdio.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_Stream);
LT_DECLARE_CLASS(LT_FileStream);

LT_FileStream* LT_FileStream_new(char* filename);
LT_FileStream* LT_FileStream_newForInput(char* filename);
LT_FileStream* LT_FileStream_newForOutput(char* filename);
LT_FileStream* LT_FileStream_newForAppending(char* filename);
LT_FileStream* LT_FileStream_newForFILE(FILE* file,
                                        int owns_file,
                                        int readable,
                                        int writable);
LT_FileStream* LT_FileStream_newForOwnedFILE(FILE* file,
                                             int readable,
                                             int writable);
LT_FileStream* LT_FileStream_newForBorrowedFILE(FILE* file,
                                                int readable,
                                                int writable);

int LT_Stream_closed(LT_Value stream);
int LT_Stream_isReadable(LT_Value stream);
int LT_Stream_isWritable(LT_Value stream);
void LT_Stream_close(LT_Value stream);
void LT_Stream_flush(LT_Value stream);
void LT_Stream_seek(LT_Value stream, long offset);
void LT_Stream_seekFromEnd(LT_Value stream, long offset);
void LT_Stream_seekFromStart(LT_Value stream, long offset);

void LT_Stream_writeByte(LT_Value stream, uint8_t byte);
void LT_Stream_writeCharacter(LT_Value stream, uint32_t codepoint);
void LT_Stream_writeByteVector(LT_Value stream, LT_ByteVector* bytevector);
void LT_Stream_writeString(LT_Value stream, LT_String* string);
void LT_Stream_writeLn(LT_Value stream);

LT_Value LT_Stream_readByte(LT_Value stream);
LT_ByteVector* LT_Stream_readBytes(LT_Value stream, size_t length);
LT_Value LT_Stream_readLine(LT_Value stream);
LT_String* LT_Stream_readString(LT_Value stream);
LT_ByteVector* LT_Stream_readByteVector(LT_Value stream);

LT__END_DECLS

#endif
