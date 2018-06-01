decode dbc message in binary format
-
for example,
```
$ python ./mt.py < shakehand.txt

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
