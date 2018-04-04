/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        ：service_message_id.h
* description    ：service message id
* date                  : 2018.01.20
* author            ：Bruce Feng
**********************************************************************************/

#ifndef _SERVICE_MESSAGE_ID_H_
#define _SERVICE_MESSAGE_ID_H_

#include <string>


#define DEFAULT_MESSAGE_NAME                              "default"
#define TIMER_CLICK_MESSAGE                                   "timer_tick"

#define VER_REQ                                                             "ver_req"
#define VER_RESP                                                           "ver_resp"

#define SHAKE_HAND_REQ                                            "shake_hand_req"
#define SHAKE_HAND_RESP                                           "shake_hand_resp"

#define CLIENT_CONNECT_NOTIFICATION                   "client_connect_notification"                        //client tcp connect remote notification
#define TCP_CHANNEL_ERROR                                      "tcp_socket_channel_error"                         //network transport error

#define STOP_TRAINING_REQ "stop_training_req"

#define CMD_AI_TRAINING_NOTIFICATION_REQ                    "cmd_start_training_req"
#define AI_TRAINING_NOTIFICATION_REQ                     "start_training_req"



#endif



