/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#ifndef H__ListTalk__UTCDateTime__
#define H__ListTalk__UTCDateTime__

#include <ListTalk/macros/decl_macros.h>

LT__BEGIN_DECLS

LT_DECLARE_CLASS(LT_UTCDateTime)

LT_Value LT_UTCDateTime_new(LT_Value year,
                            LT_Value month,
                            LT_Value day,
                            LT_Value hour,
                            LT_Value minute,
                            LT_Value second,
                            LT_Value microsecond);
LT_Value LT_UTCDateTime_year(LT_Value value);
LT_Value LT_UTCDateTime_month(LT_Value value);
LT_Value LT_UTCDateTime_day(LT_Value value);
LT_Value LT_UTCDateTime_hour(LT_Value value);
LT_Value LT_UTCDateTime_minute(LT_Value value);
LT_Value LT_UTCDateTime_second(LT_Value value);
LT_Value LT_UTCDateTime_microsecond(LT_Value value);

LT__END_DECLS

#endif
