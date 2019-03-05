/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   error.h
* description    :   error definition
* date                  :   2018.01.20
* author            :   Bruce Feng
**********************************************************************************/

#ifndef _ERROR_H_
#define _ERROR_H_

#define E_SUCCESS                                           0                   //success
#define E_EXIT_FAILURE                                      -1                  //exit because of failure
#define E_EXIT_PARSE_COMMAND_LINE                           -2                  //exit because of failure
#define E_DEFAULT                                           -3                  //default error, common use
#define E_BAD_PARAM                                         -4                  //bad param
#define E_NULL_POINTER                                      -5                  //null pointer
#define E_MSG_QUEUE_FULL                                    -6                  //message queue full
#define E_FILE_FAILURE                                      -7                  //file op failure
#define E_NOT_FOUND                                         -8                  //not found specified object
#define E_EXISTED                                           -9                  //dest already exist
#define E_INACTIVE_CHANNEL                                   -10                 //broadcast error, no active channel
#define E_NONCE                                             -11                  // nonce error
#define E_IMAGE_NOT_FOUND                                   -12                  // docker image not found
#define E_PULLING_IMAGE                                     -13
#define E_NO_DISK_SPACE                                     -14
#define E_NETWORK_FAILURE                                   -15                 //exit because network failure
#define E_BILL_DISABLE                                      -16                 //exit because network failure



#endif

