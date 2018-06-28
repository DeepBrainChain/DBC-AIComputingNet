#!/usr/bin/env python
import sys
import unittest
import types

from pprint import pprint
from matrix.ttypes import *
from matrix.ttypes_header import *
from matrix.dbc_service import *
from thrift.protocol.TBinaryProtocol import TBinaryProtocol
from thrift.transport.TTransport import TMemoryBuffer
from socket import *
import struct
import mt
import binascii

# redefine STOP mark from 0x00 to 0x7f


if __name__=="__main__":
    m = TMemoryBuffer()
    p = TBinaryProtocol(m)
    #magic = None, msg_name = None, nonce = None, session_id = None, exten_info = None
    magic=-506355561
    msg_name="shake_hand_req"
    head=msg_header(magic,msg_name)
    head.write(p)


    req = shake_hand_req()
    req.write(p)

    p.writeMessageEnd()
    m.flush()

    code = struct.pack('!L', m.getvalue().__len__()+8)
    code += struct.pack('!L',0)
    values=m.getvalue()
    i = 0
    while i < values.__len__():
        code = code + struct.pack('!s', values[i])
        i=i+1
    print(code.__len__())

    send_s = binascii.hexlify(code)
    print(send_s)

    Host = '35.231.146.70'
    PORT = 21107
    BUFSIZE = 10240
    ADDR = (Host, PORT)
    tcpclisock = socket(AF_INET, SOCK_STREAM)
    tcpclisock.connect(ADDR)
    tcpclisock.send(code)

    rd = tcpclisock.recv(BUFSIZE)
    if not rd:
        print("error")
    print(rd)
    recv_s = binascii.hexlify(rd)
    print(recv_s)
    mt.decode2(rd,len(rd))


