/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        ：error.h
* description    ：error definition
* date                  : 2018.01.20
* author            ：Bruce Feng
**********************************************************************************/

#ifndef _ERROR_H_
#define _ERROR_H_

#define E_SUCCESS                                                0                       //success
#define E_EXIT_FAILURE                                      -1                       //exit because of failure
#define E_EXIT_PARSE_COMMAND_LINE           -2                       //exit because of failure
#define E_DEFAULT                                               -3                      //default error, common use
#define E_BAD_PARAM                                         -4                     //bad param
#define E_NULL_POINTER                                    -5                      //null pointer
#define E_MSG_QUEUE_FULL                               -6                       //message queue full


#endif

