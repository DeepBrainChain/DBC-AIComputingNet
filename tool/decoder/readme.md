decode dbc message in binary format
-
for example,
```
# python ./mt.py < shakehand.txt

msg: 00 00 00 27 00 00 00 00 08 00 01 E1 D1 A0 97 0B 00 02 00 00 00 0F 73 68 61 6B 65 5F 68 61 6E 64 5F 72 65 73 70 7F 7F

packet header: 
{   'len': 39,
    'protocol_type': 0
}
msg header: 
{   'exten_info': None,
    'magic': -506355561,
    'msg_name': u'shake_hand_resp',
    'nonce': None,
    'session_id': None}
body: 
{   }
```

parse dbc matrix log file
- 
for example, parsing log of a running dbc service.
```
# tail -f ../../output/peer2/matrix_core_0.log | python ./dbc_log_parser.py

2018-06-04 21:55:32.291479 | 0x0000700005077000 | debug   | tcp socket channel destroyed,  socket type: client socket id: 270
2018-06-04 21:55:41.637491 | 0x0000700004f71000 | debug   | socket channel handler send msg: shake_hand_req socket type: client socket id: 224
2018-06-04 21:55:41.637783 | 0x0000700004f71000 | debug   |  socket type: client socket id: 224tcp socket channel tcp_socket_channel::async_write  socket type: client socket id: 224 send buf: 00 00 00 26 00 00 00 00 08 00 01 E1 D1 A0 97 0B 00 02 00 00 00 0E 73 68 61 6B 65 5F 68 61 6E 64 5F 72 65 71 7F 7F

packet header: 
{   'len': 38,
    'protocol_type': 0
}
msg header: 
{   'exten_info': None,
    'magic': -506355561,
    'msg_name': u'shake_hand_req',
    'nonce': None,
    'session_id': None}
body: 
{   }


2018-06-04 21:55:41.690087 | 0x0000700004f71000 | debug   | tcp socket channel  socket type: client socket id: 224 recv buf: 00 00 00 27 00 00 00 00 08 00 01 E1 D1 A0 97 0B 00 02 00 00 00 0F 73 68 61 6B 65 5F 68 61 6E 64 5F 72 65 73 70 7F 7F

packet header: 
{   'len': 39,
    'protocol_type': 0
}
msg header: 
{   'exten_info': None,
    'magic': -506355561,
    'msg_name': u'shake_hand_resp',
    'nonce': None,
    'session_id': None}
body: 
{   }
```

for example, save parsed log
```
# cat ../../output/peer1/matrix_core_0.log | python ./dbc_log_parser.py  > peer1_21107.log
# vim ./peer1_21107.log
```