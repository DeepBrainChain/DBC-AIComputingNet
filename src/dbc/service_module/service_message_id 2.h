#ifndef DBC_SERVICE_MESSAGE_ID_H
#define DBC_SERVICE_MESSAGE_ID_H

#include <iostream>
#include <string>

#define DEFAULT_MESSAGE_NAME                           "default"
#define TIMER_TICK_NOTIFICATION                        "time_tick_notification"

#define VER_REQ                                        "ver_req"
#define VER_RESP                                       "ver_resp"

#define SHAKE_HAND_REQ                                 "shake_hand_req"
#define SHAKE_HAND_RESP                                "shake_hand_resp"

#define CLIENT_CONNECT_NOTIFICATION                    "client_connect_notification"   //client tcp connect remote notification
#define TCP_CHANNEL_ERROR                              "tcp_socket_channel_error"      //network transport error

#define CMD_AI_TRAINING_NOTIFICATION_REQ               "cmd_start_training_req"
#define CMD_GET_PEER_NODES_REQ						   "cmd_get_peer_nodes_req"
#define CMD_GET_PEER_NODES_RESP						   "cmd_get_peer_nodes_resp"
#define P2P_GET_PEER_NODES_REQ						   "get_peer_nodes_req"
#define P2P_GET_PEER_NODES_RESP						   "get_peer_nodes_resp"
#define GET_TASK_QUEUE_SIZE_REQ                        "get_task_queue_size_req"
#define GET_TASK_QUEUE_SIZE_RESP                       "get_task_queue_size_resp"

#define BINARY_FORWARD_MSG                  "binary_forward_message"

#endif



