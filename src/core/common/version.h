/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   version.h
* description    :   core version
* date                  :   2018.02.02
* author            :   Bruce Feng
**********************************************************************************/
#pragma once

//major version . minor version. revision version. build version
//version should be revised when official release
#define CORE_VERSION                        0x00020300                              //00.02.03.00
#define PROTOCO_VERSION                     0x00000001

#define STR_CONV(v)  #v 
//STR_VER(CORE_VERSION)
#define STR_VER(v)  STR_CONV(v)