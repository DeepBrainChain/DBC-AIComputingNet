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

# redefine STOP mark from 0x00 to 0x7f


if __name__=="__main__":
    # private_key = random_key()
    # pub_key = privkey_to_pubkey(private_key)
    # pub_key_bin = binascii.a2b_hex(pub_key)
    # pub_160=bin_hash160(pub_key)
    #
    # node_prefix = bytes("node.0.")
    # node_gen_src=node_prefix+pub_160
    # node_id = base58.b58encode_check(node_gen_src)

    sstr="00 00 02 08 00 00 00 00 08 00 01 E1 D1 A0 97 0B 00 02 00 00 00 12 73 74 61 72 74 5F 74 72 61 69 6E 69 6E 67 5F 72 65 71 0B 00 03 00 00 00 21 41 76 4E 78 4D 36 76 36 42 52 46 53 6D 31 6E 4B 4E 52 53 33 64 66 58 69 4A 7A 56 6A 31 51 77 58 46 7F 0B 00 01 00 00 00 21 47 55 33 35 34 59 61 57 65 65 68 6E 56 45 4E 4E 64 78 36 64 58 4B 34 75 33 32 44 4B 67 34 31 4B 45 03 00 02 00 0B 00 03 00 00 00 2C 32 67 66 70 70 33 4D 41 42 34 34 4C 56 4D 4E 41 47 46 67 51 6D 31 69 37 72 35 6A 6D 41 79 32 57 32 47 79 45 61 6F 79 54 71 35 54 0A 0F 00 04 0B 00 00 00 01 00 00 00 2C 32 67 66 70 70 33 4D 41 42 34 34 4C 56 4D 4E 41 47 46 67 51 6D 31 69 37 72 35 6A 6D 41 79 32 57 32 47 79 45 61 6F 79 54 71 35 54 0A 0B 00 05 00 00 00 02 34 0A 08 00 06 00 00 00 21 0B 00 07 00 00 00 13 64 62 63 74 72 61 69 6E 69 6E 67 2F 68 32 6F 2D 67 70 75 0B 00 08 00 00 00 2F 51 6D 58 56 31 6E 42 38 56 44 79 32 59 4C 79 34 50 66 44 54 4C 69 50 48 46 42 7A 6E 38 4E 4B 4D 48 64 77 32 73 75 43 68 69 32 41 68 4A 61 0A 0B 00 09 00 00 00 09 73 74 61 72 74 2E 73 68 0A 0B 00 0A 00 00 00 2F 51 6D 59 55 62 55 31 34 32 41 70 6D 55 42 41 63 31 4C 46 4A 70 4E 31 72 62 62 74 36 71 38 33 36 42 55 38 62 62 6F 4A 6E 63 39 6F 54 5A 47 0A 0B 00 0B 00 00 00 36 2F 69 70 66 73 2F 58 4C 59 6B 67 71 36 31 44 59 61 51 38 4E 68 6B 63 71 79 55 37 72 4C 63 6E 53 61 37 64 53 48 51 31 36 78 2F 64 61 74 61 2F 6F 75 74 70 75 74 0A 0B 00 0C 00 00 00 35 62 61 74 63 68 5F 73 69 7A 65 3A 33 32 3B 6C 65 61 72 6E 69 6E 67 5F 72 61 74 65 3A 30 2E 30 30 31 3B 6C 65 61 72 6E 69 6E 67 5F 72 61 74 65 5F 64 65 63 61 79 7F"
    mt.decode(sstr)

    while j <100000000:
        m = TMemoryBuffer()
        p = TBinaryProtocol(m)
        #magic = None, msg_name = None, nonce = None, session_id = None, exten_info = None
        magic=-506355561
        msg_name="ver_req"
        head=msg_header(magic,msg_name)
        head.write(p)

        addr_me = network_address("127.0.0.1", 21107)
        addr_you = network_address("127.0.0.1", 21107)
        core_version = 0x00020200
        pro_version = 0x00000001
        time_stamp = 201801111223
        start_height = 1
        nodeid = str(j)
        j=j+1
        req = ver_req_body(nodeid, core_version, pro_version, time_stamp, addr_me, addr_you, start_height)
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
        Host = '10.10.254.187'
        PORT = 21107

        dbc_client = TcpClient(Host, PORT)
        dbc_client.connect()
        dbc_client.start()
        dbc_client.PutInData(code)
        # while 1:
        #     if dbc_client.IsHaveData() != 0:
        #         rev_data = dbc_client.GetData()
        #         mt.decode2(rev_data, len(rev_data))
        #         break

        # Host = '10.10.254.200'
        # PORT = 21107
        # BUFSIZE = 10240
        # ADDR = (Host, PORT)
        # tcpclisock = socket(AF_INET, SOCK_STREAM)
        # tcpclisock.connect(ADDR)
        # #tcpclisock.send(code)
        #
        # rd = tcpclisock.recv(BUFSIZE)
        # if not rd:
        #     print("error")
        # print(rd)
        # recv_s = binascii.hexlify(rd)
        # print(recv_s)
        # #mt.decode2(rd,len(rd))
        # j=j+1


