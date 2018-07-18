#!/usr/bin/env python

from bitcoin import *
import base58

VER_REQ                           =                        "ver_req"
VER_RESP                          =                        "ver_resp"
SHAKE_HAND_REQ                    =                        "shake_hand_req"
SHAKE_HAND_RESP                   =                        "shake_hand_resp"
CLIENT_CONNECT_NOTIFICATION       =                        "client_connect_notification"                      #client tcp connect remote notification
TCP_CHANNEL_ERROR                 =                        "tcp_socket_channel_error"                         #network transport error
STOP_TRAINING_REQ                 =                         "stop_training_req"
LIST_TRAINING_REQ                 =                         "list_training_req"
LIST_TRAINING_RESP                =                         "list_training_resp"
LOGS_REQ                          =                          "logs_req"
LOGS_RESP                         =                          "logs_resp"
CMD_AI_TRAINING_NOTIFICATION_REQ  =                          "cmd_start_training_req"
AI_TRAINING_NOTIFICATION_REQ      =                          "start_training_req"
AI_TRAINGING_NOTIFICATION_RESP    =                          "start_training_resp"
CMD_GET_PEER_NODES_REQ			  =	                      	 "cmd_get_peer_nodes_req"
CMD_GET_PEER_NODES_RESP			  =		                     "cmd_get_peer_nodes_resp"
P2P_GET_PEER_NODES_REQ			  =		                     "get_peer_nodes_req"
P2P_GET_PEER_NODES_RESP			  =		                     "get_peer_nodes_resp"
SHOW_REQ                          =                           "show_req"
SHOW_RESP                         =                            "show_resp"
SERVICE_BROADCAST_REQ             =                            "service_broadcast_req"
GET_TASK_QUEUE_SIZE_REQ           =                            "get_task_queue_size_req"
GET_TASK_QUEUE_SIZE_RESP          =                             "get_task_queue_size_resp"
core_version = 0x00020200
pro_version = 0x00000001
magic = -506355561
start_height=1

def get_random_id():
    private_key = random_key()
    pub_key = privkey_to_pubkey(private_key)
    pub_key_bin = binascii.a2b_hex(pub_key)
    pub_160 = bin_hash160(pub_key)
    random_id = base58.b58encode_check(pub_160)
    return random_id

def get_random_name():
    seed = "1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()_+=-"
    sa = []
    for i in range(8):
        sa.append(random.choice(seed))
    salt = ''.join(sa)
    return salt

def gen_node_id():
    private_key = random_key()
    pub_key = privkey_to_pubkey(private_key)
    pub_key_bin = binascii.a2b_hex(pub_key)
    pub_160 = bin_hash160(pub_key)
    node_prefix = bytes("node.0.")
    node_gen_src = node_prefix + pub_160
    return base58.b58encode_check(node_gen_src)
