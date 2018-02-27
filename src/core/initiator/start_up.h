/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        ：start_up.h
* description    ：core start up
* date                  : 2017.01.23
* author            ：Bruce Feng
**********************************************************************************/

#pragma once

#include "common.h"

#ifdef __RTX
extern volatile U64 stk_main[1024 / 8];             //1K stack size

__BEGIN_DECLS
extern __task void main_task();
__END_DECLS

#else

__BEGIN_DECLS
extern int main_task(int argc, char* argv[]);
__END_DECLS

#endif



