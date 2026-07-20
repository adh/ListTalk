/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 - 2026 Ales Hakl
 */

#include <ListTalk/ListTalk.h>
#include <ListTalk/utils.h>

void LT__init(void){
    LT_run_registered_constructors();
}
