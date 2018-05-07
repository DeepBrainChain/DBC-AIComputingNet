/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        ：common.h
* description    ：common header file
* date                  : 2018.01.20
* author            ：Bruce Feng
**********************************************************************************/

#ifndef _COMMON_H_
#define _COMMON_H_

#include <string.h>
#include <stdlib.h>

#if defined(WIN32) || defined(__linux__) || defined(MAC_OSX)

#include <cassert>
#include <memory>

#endif

#ifdef __RTX
#include "rtl.h"
#include "cs_string.h"
#endif

#include "error.h"
#include "std_types.h"
#include "visibility.h"
#include "common_def.h"
#include "log.h"

//#pragma warning(disable : 4996)


#endif /* _COMMON_H_ */


