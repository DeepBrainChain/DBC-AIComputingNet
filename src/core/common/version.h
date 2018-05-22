/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºversion.h
* description    £ºcore version
* date                  : 2018.02.02
* author            £ºBruce Feng
**********************************************************************************/
#pragma once

//major version . minor version. bug fix version
//version should be revised when official release
#define CORE_VERSION                        0x00000200                              //0000.02.00
#define STR_CONV(v)  #v 
//STR_VER(CORE_VERSION)
#define STR_VER(v)  STR_CONV(v)