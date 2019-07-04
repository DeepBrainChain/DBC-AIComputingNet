

This document depicts some major design concepts of dbc network:
* [dbc matrix protocol](#protocol)
* [security](#security)
* [RESTFul API] (#RESTFul API)


# dbc matrix protocol
Each message contains a external packet header, a msg header and a msg body.

packet.protocol_type indicates the coding scheme of the inner message:
* 0: THRIFT_BINARY_PROTO
* 1: THRIFT_COMPACT_PROTO
* 257:  THRIFT_COMPACT_PROTO + SNAPPY_RAW

dbc applies both compact and compression to save the network bandwidth. THRIFT_BINARY_PROTO is the basic thrift coding scheme. THRIFT_COMPACT_PROTO decrease the message length through compacting method.
SNAPPY is a efficient compression method. 

dbc node will negotiate the encoding method with the neighbour/peer through ver_req/ver_resp, where dbc node send all encoding scheme suppported.

For the message marked as relayable message, dbc node needs to relay it to all its neighbour nodes after message received.
get_peer_nodes_resp will be treated as relayable message if it only contains one public ip. 

## dbc matrix message list
none relayable message
* ver_req
* ver_resp
* shake_hand_req
* shake_hand_resp
* service_broadcast_req
* get_peer_nodes_resp

relayable message
* start_training_req 
* stop_training_req
* list_training_req 
* show_req
* show_resp
* get_peer_nodes_resp (with single public ip)
* logs_req
* logs_resp



### ver_req
node send ver_req to its neighbour with basic local node info.
```
packet header: 
{   'len': 480,
    'protocol_type': 0
}
msg header: 
{   'exten_info': {   u'capacity': u'thrift_binary;thrift_compact;snappy_raw',
                      u'sign': u'1fc023fb128f2c97454c6d204a3adc6d5505a3001885e98a4c5a47694bb1554a3a5fa3b750cb3dbab7a4784db1300aad5aaef8e9d054db0d7fa1da6bfa6b72a7e4',
                      u'sign_algo': u'ecdsa',
                      u'sign_at': u'1562141958'},
    'magic': -506355559,
    'msg_name': u'ver_req',
    'nonce': u'26xW4GN8MMaUJhFH5T1E8FoLcxgjUWSiPQgTeXdth3ADK1EkHh',
    'path': None,
    'session_id': None}
body: 
{   'addr_me': network_address(ip=u'0.0.0.0', port=21107),
    'addr_you': network_address(ip=u'114.116.21.175', port=21107),
    'core_version': 198151,
    'node_id': u'2gfpp3MAB475q4WNyyuL6kPUjw3nedJLKRgiiJiUj7Z',
    'protocol_version': 1,
    'start_height': 0,
    'time_stamp': 1562141958}
    
```

### ver_resp
response of ver_req, with basic local node info.
```
packet header:
{   'len': 352,
    'protocol_type': 257
}
msg header:
{   'exten_info': {   u'capacity': u'thrift_binary;thrift_compact;snappy_raw',
                      u'sign': u'1f5a0dd9c166aec5bc73a31456e7ee33057e825a20f970561ebb106316160b84d95333b7cf63a1badf37198e1b9dd3a58113a3e729eafc831c640b5c7f6fb9f0a0',
                      u'sign_algo': u'ecdsa',
                      u'sign_at': u'1562141958'},
    'magic': -506355559,
    'msg_name': u'ver_resp',
    'nonce': u'FoKDbdmz5Kuz8KFJrv8jzxmLR1XvtxzE58ofYfXxpts1hHjuC',
    'path': None,
    'session_id': None}
body:
{   'core_version': 198149,
    'node_id': u'2gfpp3MAB4F8a5dTsjo8qduNuWoQVWw1XJeRYL7rKiC',
    'protocol_version': 1}
```

### shake_hand_req
keep alive
```
packet header:
{   'len': 32,
    'protocol_type': 1
}
msg header:
{   'exten_info': None,
    'magic': -506355559,
    'msg_name': u'shake_hand_req',
    'nonce': None,
    'path': None,
    'session_id': None}
body:
{   }
```

### shake_hand_resp
keep alive
```
packet header:
{   'len': 33,
    'protocol_type': 1
}
msg header:
{   'exten_info': None,
    'magic': -506355559,
    'msg_name': u'shake_hand_resp',
    'nonce': None,
    'path': None,
    'session_id': None}
body:
{   }
```

### get_peer_nodes_resp
This message contains the dbc nodes with public IP. get_peer_nodes_resp is not relayable except one special case: peer_node_list has only one member. In this case the message is relayable. 
```
packet header:
{   'len': 239,
    'protocol_type': 1
}
msg header:
{   'exten_info': None,
    'magic': -506355559,
    'msg_name': u'get_peer_nodes_resp',
    'nonce': u'2JDUpPTR2BpsgV52zRQD8Up3f1zbvGzZ2fzTSih3kWKZTQhX7k',
    'path': None,
    'session_id': None}
body:
{   'peer_nodes_list': [   peer_node_info(addr=network_address(ip=u'111.44.254.140', port=21107), core_version=0, peer_node_id=u'2gfpp3MAB4Ajzk3CBxFJemGCGQ9VsquKN8tLBUosmqz', service_list=None, live_time_stamp=0, protocol_version=0),
                           peer_node_info(addr=network_address(ip=u'111.44.254.132', port=21107), core_version=0, peer_node_id=u'2gfpp3MAB3xSmJEvJEGDRztbFuznDZyqZhEJeaCNJM4', service_list=None, live_time_stamp=0, protocol_version=0)]}
```


### start_training_req
This message request remote computing node to start a specific GPU server. 
```
packet header:
{   'len': 760,
    'protocol_type': 257
}
msg header:
{   'exten_info': {   u'dest_id': u'2gfpp3MAB3yhZdCs6vB5Zb5xfa2t7bCRJW45RcK7dvc ',
                      u'origin_id': u'2gfpp3MAB4JHVZUGNTaiP8xkXWQUARKjhgykorMmbFY',
                      u'sign': u'209fe071f90c7eba8dabe588364f7a0cc1d03b7e6fb29d3a910ad53f0e9251cd1e4c648ccbb71674aabb6f4e38d27e7a93b2cdf02ca203cac86fd82155655de879',
                      u'sign_algo': u'ecdsa',
                      u'sign_at': u'1562142099'},
    'magic': -506355559,
    'msg_name': u'start_training_req',
    'nonce': u'PzhcgNit788suhS4teBNBg9qvNJJsG1As2j4qsbXX8U8SdRKD',
    'path': None,
    'session_id': None}
body:
{   'checkpoint_dir': u'',
    'code_dir': u'QmVe8b9F3Xikjmjx3tCbwww81ZUk1Hz9s3PNatwpKBGytP',
    'container_name': u'',
    'data_dir': u'',
    'entry_file': u'run.sh',
    'hyper_parameters': u'{"ngrok_token": "asus_test2_j", "ngrok_server": [{"ip": "114.115.219.202", "dn": "2.ngrok.dbc.com:20443"}], "services": {"ssh":"22","http":"3000","jupyter":"20003"}, "callbackURL":"47.98.254.232/dbc"}',
    'master': u'',
    'peer_nodes_list': [   u'2gfpp3MAB3yhZdCs6vB5Zb5xfa2t7bCRJW45RcK7dvc'],
    'select_mode': 0,
    'server_count': 0,
    'server_specification': u'{"env":{"NVIDIA_VISIBLE_DEVICES":"0"}}',
    'task_id': u'2eNJSgaMfCL2pfSLv5YqM7qw4fM4nJt7uGnUc4qvhCdYxLvfee',
    'training_engine': u'liushihhao/dbc_tensorflow:v2'}
```

### stop_training_req
Stop a GPU server in the remove node. 
```
packet header:
{   'len': 370,
    'protocol_type': 257
}
msg header:
{   'exten_info': {   u'origin_id': u'2gfpp3MAB475q4WNyyuL6kPUjw3nedJLKRgiiJiUj7Z',
                      u'sign': u'1f9ad2c9537a0a4a3fdc3cd0185fce397bbc7d9d22de2a2379cdd1320188d5e28c47b635bad4e25e725951a31777dbc892010d7448f5c60f6145f3b61e6496a405',
                      u'sign_algo': u'ecdsa',
                      u'sign_at': u'1562141985'},
    'magic': -506355559,
    'msg_name': u'stop_training_req',
    'nonce': u'RzspPVQBF497RvtgYBcmdwpEKGCsLdf2pUiWBKKscbbW7oaJt',
    'path': None,
    'session_id': None}
body:
{   'task_id': u'BeMTHNeUogSQNkLhZRDWZ2kyXJcBaSf1bTrmT6WuoAqeGS5Xx'}
```

### list_training_req
```
packet header:
{   'len': 431,
    'protocol_type': 257
}
msg header:
{   'exten_info': {   u'origin_id': u'2gfpp3MAB475q4WNyyuL6kPUjw3nedJLKRgiiJiUj7Z',
                      u'sign': u'1f78e35f2ee483c9c399d78117864dc88e4769bb3f9c7ced25fcbf08ccc0b61e162aa5bd857297a61696c758e55fe3b9f1d2ed4fae84a53fb5c0292fe26e79f854',
                      u'sign_algo': u'ecdsa',
                      u'sign_at': u'1562141985'},
    'magic': -506355559,
    'msg_name': u'list_training_req',
    'nonce': u'2jMLCLStzVR7ZYJKNx1g896NuYJ7GmqFLcZrx8sLRMV82Tv5wM',
    'path': [u'2gfpp3MAB475q4WNyyuL6kPUjw3nedJLKRgiiJiUj7Z'],
    'session_id': u'VAvzizckJhiHbwGpKikkqPMSmwzG4xRjz4FM7eY2h9k6MDexA'}
body:
{   'task_list': [   u'BeMTHNeUogSQNkLhZRDWZ2kyXJcBaSf1bTrmT6WuoAqeGS5Xx']}
```

### list_training_resp
```
packet header:
{   'len': 429,
    'protocol_type': 257
}
msg header:
{   'exten_info': {   u'origin_id': u'2gfpp3MAB4ARp8JAkTBjPyEUdPM2wckxuDgHNQ3AxKA',
                      u'sign': u'20e3d7a26c4d1f08b0f9e642397f8e3564d479b23517daf60b28e1ec50b5392b606d3ab4684fba653d029ea3430034083fa7b1c1e2f7d178d64b842df242fc82e0',
                      u'sign_algo': u'ecdsa',
                      u'sign_at': u'1562141987'},
    'magic': -506355559,
    'msg_name': u'list_training_resp',
    'nonce': u'166vaRuwLAr5F7fiLJxGpmBZZFPEgDxga1ZMDxUNT6ANj3uqb',
    'path': [],
    'session_id': u'VAvzizckJhiHbwGpKikkqPMSmwzG4xRjz4FM7eY2h9k6MDexA'}
body:
{   'task_status_list': [   task_status(status=8, task_id=u'BeMTHNeUogSQNkLhZRDWZ2kyXJcBaSf1bTrmT6WuoAqeGS5Xx')]}
```

### show_req
query a certain info from remote node
```
packet header:
{   'len': 405,
    'protocol_type': 257
}
msg header:
{   'exten_info': {   u'sign': u'1fa595b6c280389090231a7cfa545c3467fcf389c11b6ee89c40953615d7dc89a92c2d535c9a96a2d7178e893ed681145d8520d27b6d03007e30763a3eecbc434d',
                      u'sign_algo': u'ecdsa',
                      u'sign_at': u'1562141978'},
    'magic': -506355559,
    'msg_name': u'show_req',
    'nonce': u'ukJiZmgaX8qAASbiZZ52WC28Adi3PDdic4cYAGnDhj1EGRDgj',
    'path': [u'2gfpp3MAB475q4WNyyuL6kPUjw3nedJLKRgiiJiUj7Z'],
    'session_id': u'2pzx6BeHJcih4ZTM78zQ1xfE3FnZ3q5JyDHwsQnx9qArPvcK1z'}
body:
{   'd_node_id': u'2gfpp3MAB4ARp8JAkTBjPyEUdPM2wckxuDgHNQ3AxKA',
    'keys': [   u'gpu_state'],
    'o_node_id': u'2gfpp3MAB475q4WNyyuL6kPUjw3nedJLKRgiiJiUj7Z'}
``` 

### show_resp
```
packet header:
{   'len': 511,
    'protocol_type': 257
}
msg header:
{   'exten_info': {   u'sign': u'209551d967006993eb433fbd4e708cd42954e0edabece432f9b6587054da00d85920180181e1ed89e689498567a715b58fe83eec58579fa8f7709e921b27f3ed97',
                      u'sign_algo': u'ecdsa',
                      u'sign_at': u'1562141978'},
    'magic': -506355559,
    'msg_name': u'show_resp',
    'nonce': u'pGgBSGMNHKVA3hsSx14m9KwRQx38g7V96zw2vjZ1BXGCbgp81',
    'path': [],
    'session_id': u'2pzx6BeHJcih4ZTM78zQ1xfE3FnZ3q5JyDHwsQnx9qArPvcK1z'}
body:
{   'd_node_id': u'2gfpp3MAB475q4WNyyuL6kPUjw3nedJLKRgiiJiUj7Z',
    'kvs': {   u'gpu_state': u'{"gpus":[{"id":"0","state":"idle","type":"GeForce 940MX","uuid":"GPU-914b7cac-4d5f-60a9-7abb-aee06a91176c"}]}'},
    'o_node_id': u'2gfpp3MAB4ARp8JAkTBjPyEUdPM2wckxuDgHNQ3AxKA'}
```

### service_broadcast_req
Each node needs to send the cached node id of the whole dbc network to its neighbours.
This message will be sent in fixed interval, e.g. 30 seconds. And message segment is used if the cached nodes info beyond the message length limit. 
```
packet header:
{   'len': 1283,
    'protocol_type': 257
}
msg header:
{   'exten_info': None,
    'magic': -506355559,
    'msg_name': u'service_broadcast_req',
    'nonce': u'2sMgkidH2TGJK3orGFgZh27dCb4HBgwF4NggFDNYNYoyy2Epes',
    'path': None,
    'session_id': u''}
body:
{   'node_service_info_map': {   u'2gfpp3MAB3w5vHsvd7uNhz7zKy3WYgwf2Ezj1ddWoky': node_service_info(service_list=[u'ai_training'], time_stamp=1562141958, name=u'4cars-003', kvs={u'startup_time': u'1561458208', u'gpu_usage': u'0 %', u'state': u'busy(1)', u'version': u'0.3.6.7', u'gpu': u'4 * GeForceGTX1080Ti', u'gpu_state': u'N/A'}),
                                 u'2gfpp3MAB3xSmJEvJEGDRztbFuznDZyqZhEJeaCNJM4': node_service_info(service_list=[u'ai_training'], time_stamp=1562141940, name=u'4cars-002', kvs={u'startup_time': u'1559726594', u'gpu_usage': u'0 %', u'state': u'busy(2)', u'version': u'0.3.6.4', u'gpu': u'4 * GeForceGTX1080Ti', u'gpu_state': u'N/A'}),
                                 u'2gfpp3MAB3yhZdCs6vB5Zb5xfa2t7bCRJW45RcK7dvc': node_service_info(service_list=[u'ai_training'], time_stamp=1562141929, name=u'lei-System-Produ', kvs={u'startup_time': u'1562136667', u'gpu_usage': u'0 %', u'state': u'busy(1)', u'version': u'0.3.6.5', u'gpu': u'1 * GeForceGTX1080', u'gpu_state': u'N/A'}),
                                 u'2gfpp3MAB41zhAmTuWkdDigxeQcCKdfxof3RtyLKYNn': node_service_info(service_list=[u'ai_training'], time_stamp=1562141936, name=u'j1-g04-2-4k', kvs={u'startup_time': u'1561616124', u'gpu_usage': u'0 %', u'state': u'idle', u'version': u'0.3.6.0', u'gpu': u'4 * GeForceGTX1080Ti', u'gpu_state': u'N/A'}),
                                 u'2gfpp3MAB432txKPYAkJorw2VaGT4dfTFZUirFKnXs6': node_service_info(service_list=[u'ai_training'], time_stamp=1562141943, name=u'dbcloud-Super-Se', kvs={u'startup_time': u'1561465027', u'gpu_usage': u'62 %', u'state': u'idle', u'version': u'0.3.6.0', u'gpu': u'4 * GeForceRTX2080Ti', u'gpu_state': u'N/A'}),
...
```

### logs_req
```
packet header: 
{   'len': 501,
    'protocol_type': 257
}
msg header: 
{   'exten_info': {   u'origin_id': u'2gfpp3MAB4JHVZUGNTaiP8xkXWQUARKjhgykorMmbFY',
                      u'sign': u'20e5a6552a27d4d967770b1f8010f70fcf412f61e2c75aff4bd143216ca63e7a596093207d145784487f30a8fa5a569e1e421d752842d7873735fc81fdfea01de7',
                      u'sign_algo': u'ecdsa',
                      u'sign_at': u'1562149830'},
    'magic': -506355559,
    'msg_name': u'logs_req',
    'nonce': u'5xbCDb6fJucT1Zhcgz48JWYLh2bZGLEzRKrAGKXDdsKrjgxdQ',
    'path': [   u'2gfpp3MAB4JHVZUGNTaiP8xkXWQUARKjhgykorMmbFY',
                u'2gfpp3MAB4F8a5dTsjo8qduNuWoQVWw1XJeRYL7rKiC',
                u'2gfpp3MAB475q4WNyyuL6kPUjw3nedJLKRgiiJiUj7Z'],
    'session_id': u'5hjLJrLmVRz74fXKjPCinuGBMeaV6TUuHn5HnzfUNb3xgWxzZ'}
body: 
{   'head_or_tail': 1,
    'number_of_lines': 10,
    'peer_nodes_list': [   ],
    'task_id': u'2eNJSgaMfCL2pfSLv5YqM7qw4fM4nJt7uGnUc4qvhCdYxLvfee'}
```

### logs_resp
To protect user info, log_content field is encrypted with ECDH.
```
packet header:
{   'len': 3541,
    'protocol_type': 257
}
msg header:
{   'exten_info': {   u'ecdh_pub': '\x03\xa6c\xac\x1c\xe6d\xf16\x9da\xbb\xf2*\x8aZ\xfc\x10\xf0\xb5\tu\xdaH\xd9\xaa\x16-+\xed\x8d\xe5\xb8',
                      u'sign': u'1fac6786eb5afd6e6049cc26bbc7ef2f306378700f72c2995419d70ecf4af7b2c603b0370c72aa327f7f3090aa491e3afa04e8b681301eb00981b610cbe2094735',
                      u'sign_algo': u'ecdsa',
                      u'sign_at': u'1562150476'},
    'magic': -506355559,
    'msg_name': u'logs_resp',
    'nonce': u'Rz9Cnn24UkoiD4WQsLkm6CNcRGU8GxgiDB4ooCnHWciAHsxag',
    'path': [],
    'session_id': u'22Zz9DySf5KvMfNwX92zYJFCPi9UfZrY8zryn4XDDL4Zmwn622'}
body:
{   'log': peer_node_log(peer_node_id=u'2gfpp3MAB4ARp8JAkTBjPyEUdPM2wckxuDgHNQ3AxKA', log_content=' \xd7,C\rn*\x8d?\x8d\xd0\xc7\x99\xb1\xde\x14u\x0f\xa5\x88\xb2\xacB\x85{\xa9\'~\xf90\xe2\xed9\x18\x0c&\xdd\x02\xeb7b\x0e
    ...
```
    

# security

Major security threat in dbc network and the related hardening tech.
* using computing resource without authority
    * authority   
* fake message
    * signature
* data security of GPU Server
    * ssh 
    * scp
    * encryption
* message replay
    * nonce 
* network broadcast storm
    * message threshold per connection
* comsume all connection of seed node
    * dynamic connection adjustment 

## authority 
Every node can send request to ask the computing node to create a GPU Server. So the computing node needs to check the received request if the original node is authorized.

There are three authority strategy:
* no auth
* offline
* online

With offline auth, the computing node owner needs to add "trust_node_id" into core.conf file.
With online auth, dbc will send RESTFul API to external billing node.


## signature
The original dbc node always send message with a signature of this message. 
Other node can extract public key from the signature, and then verify both the message content and the message's original source node id. 

```
How to generate dbc node id from permanent public key?
    secp256k1_pubkey 
            | |
            | |  serialize
            | |
    CPubKey (better for network transport)
            | |
            | |  base58 encoding
            | |
    node id. (better for human being read and write)
```

## encryption
message with private/confidential info will be encrypted before transport in dbc network. 

so far, only the resp message from computing node will be encrypt. node A send a request to node B to ask for some confidential inforamtion.

node B encrypt message:
 * generate a pair of onetime secect and public key (s_onetime/p_onetime). 
 * derive the permanent public key of node A, p_nodeA, from signature in the request message
 * caculate the one time share secret: ecdh(p_nodeA, s_onetime)
 * node B encrypt the confidential info with AES algorithm
 * node B send both cipher text and the p_onetime to node A
 
Finally node A decrypt the message:
 * node A caculate the onetime share secret: ecdh(p_onetime, s_nodeA)
 * decrypt the cipher text
 
Note: both s_onetime and node A's permanent secret do not transport in the network.


## ssh & scp
AI user login GPU server through ssh. And GPU server will provide a initial password and ssh login IP and port.
* GPU Server will generate an initial random password and dbc client node can get it through log_resp
* login IP and port depends  
    * if the computing node has public IP, use the public IP and mapping port 22 to port in computing node, e.g. 1022  
    * otherwise GPU Server use ngrok server as ssh proxy
    
AI user can also use scp to transport binary/data between the GPU server and local computer.


## nonce
each message has a random nonce field. The nonce is used to identify the same broadcast message transport from different path. And it also used to protect message replay attack.
Each node maintains a Bloom filter to identify if the received message is duplicated.

## threshold
Each node can set its receiving window size for each connection. This window is maintained by dbc application, and flooding message will be dropped. 
 
## connection adjustment
All node with public IP will listen for connection establishment from other nodes; and only one connection is allowed from the same node.
Seed nodes act as the entry portal, and use more strict policy, for example, limit connection number from the one subnet.
dbc node will receive address of other nodes with public IP. And then release the connection to seed node after new connetin to node with public IP address established.   



# RESTFUL API
refer to [dbc_restful.md](dbc_restful.md) for more details.
