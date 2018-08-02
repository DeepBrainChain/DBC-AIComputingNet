#!/usr/bin/env python

from bitcoin import *
from base58 import base58

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

magic = -506355567
start_height=1

private_key=""
node_id=""

def set_magic(magic_num):
    global magic
    bb = binascii.a2b_hex(magic_num)
    magic = struct.unpack('!i', bb[0:4])[0]

def get_private_key():
    return private_key

def get_node_id():
    return node_id

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

def derive_dbcprivate_key(prikey):
    # prikey="29SjBR3HHvBSPMKjTgEaQLZcP3PEGC8zgjLkAjcfpj9uESRdssM1wHpusKkoajUyqNXbdCoyXXp4bCzYzdaqUaqcYHnFh9hLzQNJ2qobncd862Uwgd47sHv82QXEmeUBEJ8yFXM58trw1RSscnXvV49bYGrbt3FDrMgN9EeJCv1AfVryab9rZgFrPbsXvcYqym4hnYU1bAgS5DUPrTaZESkQtqbu5y9CKsvDvqZKzx9ug47JpWW3Nzig88vTcBfFJ3Wqnz22iC3WdaFTwGwU8gZ2gPSE1b3LkUvS46n4SF8BS"
    bb = base58.b58decode_check(prikey)
    return binascii.hexlify(bb[10:42])

def derive_dbcprivate_key_node():
    prikey="29SjBR3HHvBSPMKjTgEaQLZcP3PEGC8zgjLkAjcfpj9uESRdssM1wHpusKkoajUyqNXbdCoyXXp4bCzYzdaqUaqcYHnFh9hLzQNJ2qobncd862Uwgd47sHv82QXEmeUBEJ8yFXM58trw1RSscnXvV49bYGrbt3FDrMgN9EeJCv1AfVryab9rZgFrPbsXvcYqym4hnYU1bAgS5DUPrTaZESkQtqbu5y9CKsvDvqZKzx9ug47JpWW3Nzig88vTcBfFJ3Wqnz22iC3WdaFTwGwU8gZ2gPSE1b3LkUvS46n4SF8BS"
    bb = base58.b58decode_check(prikey)
    t1= binascii.hexlify(bb[10:42])
    pub_key = privkey_to_pubkey(t1)
    pub_key_bin = binascii.a2b_hex(pub_key)
    pub_160 = hash160(pub_key_bin)
    keyid = binascii.a2b_hex(pub_160)
    node_prefix = bytes("node.0.")
    node_gen_src = node_prefix + keyid
    node_id = base58.b58encode_check(node_gen_src)
    print(node_id)

def gen_node_id():
    global private_key
    private_key = random_key()
    # seed = random_electrum_seed()
    # private_key=electrum_privkey(seed,1)
    pub_key = privkey_to_pubkey(private_key)
    pub_key_bin = binascii.a2b_hex(pub_key)
    pub_160 = hash160(pub_key_bin)
    keyid = binascii.a2b_hex(pub_160)
    node_prefix = bytes("node.0.")
    node_gen_src = node_prefix + keyid
    global node_id
    node_id = base58.b58encode_check(node_gen_src)
    return node_id

def dbc_sign(message):
    private_key = get_private_key()
    message_hash = bin_dbl_sha256(from_string_to_bytes(message))
    message_hash_hex = binascii.hexlify(message_hash)
    v, r, s = ecdsa_raw_sign(message_hash_hex, private_key)
    sig = encode_sig(v, r, s)
    vb, rb, sb = from_int_to_byte(v + 4), encode(r, 16), encode(s, 16)
    sign = binascii.hexlify(vb) + rb + sb
    return sign