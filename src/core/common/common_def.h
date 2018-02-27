/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        ：common_def.h
* description    ：common definition
* date                  : 2018.01.20
* author            ：Bruce Feng
**********************************************************************************/

#ifndef _COMMON_DEF_H_
#define _COMMON_DEF_H_

#include "common.h"


//#define DELETE(p)  if (NULL != p) {delete p; p = NULL;}


#ifdef  USE_FULL_ASSERT

#ifdef __RTX
__BEGIN_DECLS
extern void assert_failed(uint8_t* file, uint32_t line);
__END_DECLS;
#define ASSERT(expr)           ((expr) ? (void)0 : assert_failed((uint8_t *)__FILE__, __LINE__))
#endif

#endif

#endif /* _COMMON_DEF_H_ */


