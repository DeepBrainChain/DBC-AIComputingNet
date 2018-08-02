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
from  tcpserver import *
from bitcoin import *
import base58

from threading import Thread
from tcpclient import *
from dbc_message.msg_common import  *
from dbc_message.node_ver_req import *
# redefine STOP mark from 0x00 to 0x7f


if __name__=="__main__":
    # ss = mt.decode("00 00 00 87 00 00 00 00 08 00 01 E1 D1 A0 97 0B 00 02 00 00 00 07 76 65 72 5F 72 65 71 0B 00 03 00 00 00 21 35 4B 51 72 64 75 74 77 61 42 58 32 6F 4B 35 37 42 72 42 6E 34 67 54 6D 7A 6D 38 45 52 46 33 67 70 7F 0B 00 01 00 00 00 2B 32 67 66 70 70 33 4D 41 42 33 78 41 79 4C 4E 36 42 58 74 44 36 65 79 53 41 59 57 69 41 77 78 39 68 55 39 67 31 78 66 35 32 31 61 08 00 02 00 02 02 00 08 00 03 00 00 00 01 7F")
    j = 0
    while j <10000:
        j = j+1
        Host = '10.10.254.187'
        PORT = 21107
        nodeid = gen_node_id()

        dbc_client = TcpClient(Host, PORT)
        dbc_client.connect()
        dbc_client.start()
        #dbc_client.PutInData(make_ver_req(Host, PORT, nodeid))
        dbc_client.close()
