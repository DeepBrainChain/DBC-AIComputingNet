/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name          :   service_message_id.h
* description     :    service message id
* date                   :   2018.01.20
* author              :   Bruce Feng
**********************************************************************************/

#ifndef _SERVICE_MESSAGE_ID_H_
#define _SERVICE_MESSAGE_ID_H_

#include <string>


#define DEFAULT_MESSAGE_NAME                              "default"
#define TIMER_TICK_NOTIFICATION                        "time_tick_notification"

#define VER_REQ                                                             "ver_req"
#define VER_RESP                                                           "ver_resp"

#define SHAKE_HAND_REQ                                            "shake_hand_req"
#define SHAKE_HAND_RESP                                           "shake_hand_resp"

#define CLIENT_CONNECT_NOTIFICATION                             "client_connect_notification"                      //client tcp connect remote notification
#define TCP_CHANNEL_ERROR                                       "tcp_socket_channel_error"                         //network transport error

#define STOP_TRAINING_REQ                                       "stop_training_req"
#define LIST_TRAINING_REQ                                       "list_training_req"
#define LIST_TRAINING_RESP                                      "list_training_resp"

#define LOGS_REQ                                                 "logs_req"
#define LOGS_RESP                                            "logs_resp"

#define CMD_AI_TRAINING_NOTIFICATION_REQ                            "cmd_start_training_req"
#define AI_TRAINING_NOTIFICATION_REQ                                "start_training_req"
#define AI_TRAINGING_NOTIFICATION_RESP                              "start_training_resp"

#define CMD_GET_PEER_NODES_REQ								"cmd_get_peer_nodes_req"
#define CMD_GET_PEER_NODES_RESP								"cmd_get_peer_nodes_resp"
#define P2P_GET_PEER_NODES_REQ								"get_peer_nodes_req"
#define P2P_GET_PEER_NODES_RESP								"get_peer_nodes_resp"


#define SHOW_REQ                                                 "show_req"
#define SHOW_RESP                                                "show_resp"

#define SERVICE_BROADCAST_REQ                                    "service_broadcast_req"

#define GET_TASK_QUEUE_SIZE_REQ                                 "get_task_queue_size_req"
#define GET_TASK_QUEUE_SIZE_RESP                                "get_task_queue_size_resp"

#endif



