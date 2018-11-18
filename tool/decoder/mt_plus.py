#!/usr/bin/env python
import sys
import unittest
import types

from pprint import pprint, pformat
from thrift_py.matrix.ttypes import *
from thrift.protocol.TCompactProtocol import TCompactProtocol, CompactType
from thrift.protocol.TBinaryProtocol import TBinaryProtocol
from thrift.transport.TTransport import TMemoryBuffer

import ttypes_header
import time
import struct
import snappy

# redefine STOP mark from 0x00 to 0x7f
TType.STOP = 0x7f
CompactType.STOP = 0x7f

class DBCPacket():
    def __init__(self,s="",b=""):
        self.s = s
        self.b = b
        self.len = 0
        self.p_type = 0
        self.header = empty
        self.body = empty

        if s and not b :
            self.b = s_to_b(s)

        if not s and b :
            self.s = b_to_s(b)
    
    def __str__(self):
        _str = "\n"
        _str += "msg: "+ self.s
        _str += "\n"
        _str += "packet header: \n{   'len': %d,\n    'protocol_type': %d\n}" % (self.len, self.p_type)
        if self.header!= empty:
            _str += "\nmsg header: \n"
            _str += pformat(vars(self.header), indent=4)

            _str += "\nbody: \n"
            _str += pformat(vars(self.body), indent=4, width=24)

        return _str

    def decode(self):
        n, p_type = self.decode_packet_header(self.b)
        if n != len(self.b):
            print
            print "ERROR: packet_header.len is %d, but the input msg has %d bytes!" % (n, len(self.b))
            print

            return

        self.len, self.p_type = n, p_type
        self.decode_packet_body(self.b[8:])

    def decode_packet_header(self,p):
        # 4 bytes: len
        # 4 bytes: protocol
        proto = struct.unpack('!L', p[4:8])

        x = struct.unpack('!L', p[:4])

        return x[0], proto[0]

    def decode_packet_body(self,bs):

        if self.p_type >> 8 == 1:
            bs = self.uncompress(bs)

        thrift_ptype = self.p_type & 0xff

        if thrift_ptype == 1:
            self.decode_compact(bs)
        elif thrift_ptype == 0:
            self.decode_binary(bs)
        else:
            print "ERROR: unknow protocol type %d" %(self.p_type)

    def decode_binary(self,bs):
        m = TMemoryBuffer(bs)
        p = TBinaryProtocol(m)

        try:
            # h = msg_header()
            h = ttypes_header.msg_header()
            h.read(p)
        except EOFError:
            print "Error: msg header decode failure"
            return

        if not h.msg_name:
            print "Error: msg header decode failure"
            return

        self.header = h

        s = h.msg_name+"_body"
        if s in globals():
            t = globals()[s]
        else:
            t = empty

        try:
            body= t()
            body.read(p)
        except EOFError:
            print "Error: msg body decode failure"
            return

        self.body = body


    def decode_compact(self,bs):
        m = TMemoryBuffer(bs)   # skip packet header
        p = TCompactProtocol(m)
        try:
            # h = msg_header()
            h = ttypes_header.msg_header()
            h.read(p)
        except EOFError:
            print "Error: msg header decode failure"
            return

        self.header = h

        # print m._buffer.tell()  # buffer offset
        s = h.msg_name+"_body"
        if s in globals():
            t = globals()[s]
        else:
            t = empty

        try:
            body= t()
            body.read(p)
        except EOFError:
            print "Error: msg body decode failure"
            return

        self.body = body

    def uncompress(self,bs):
        return snappy.uncompress(bs)


def load_msg():
    t = [x.strip() for x in sys.stdin.readlines()]
    t = [x for x in t if len(x)>0] # remove empty line
    s = ' '.join(t)
    return s


def s_to_b(s):
    s = s.strip()
    s = s.split(' ')
    s = [a for a in s if a]  # remove all space symbols

    len_ = len(s)

    s = ''.join(s)
    bs = []

    try:
        n = ''
        for i in range(0, len(s), 2):
            n = s[i:i+2]
            bs.append(chr(int(n, 16)))

    except ValueError as e:
        print e,n
        exit(1)

    return ''.join(bs)


def b_to_s(b):
    c = ["%02x" %(i) for i in b]
    return ' '.join(c)


def pack_head(m):
    code = struct.pack('!L', m.getvalue().__len__() + 8)
    code += struct.pack('!L', 1)
    values = m.getvalue()
    i = 0
    while i < values.__len__():
        code = code + struct.pack('!s', values[i])
        i = i + 1
    return code

def make_ver_req(peer_ip, peer_port,node_id):
    m = TMemoryBuffer()
    # p = TBinaryProtocol(m)
    p = TCompactProtocol(m)
    msg_name = "ver_req"
    nonce = "0"  #get_random_id()
    head = ttypes_header.msg_header(0, msg_name, nonce)
    head.write(p)
    # print("nonce:%s, node_id:%s" %(nonce,  node_id))
    addr_me = network_address("127.0.0.1", 1)
    addr_you = network_address(peer_ip, peer_port)
    time_stamp = int(time.time())
    req = ver_req_body(node_id, 0, 0, time_stamp, addr_me, addr_you, 1)

    # p.writeMessageBegin()
    req.write(p)
    # p.writeMessageEnd()
    m.flush()

    v = m.getvalue()
    # print v
    # print [hex(ord(i)) for i in v]

    c = pack_head(m)
    # pprint(c)

    # print [hex(ord(i)) for i in c]
    return c


class TestEncode(unittest.TestCase):

    def test_ver_req(self):
        c = make_ver_req("127.0.0.1", 129, "0");
        s = [format('%02x' %(ord(i))) for i in c]
        msg = ' '.join(s)

        p = DBCPacket(msg)
        p.decode()
        print p



class TestDecode(unittest.TestCase):

    def test_thrift_binary(self):
        msg = '00 00 00 27 00 00 00 00 08 00 01 E1 D1 A0 98 0B 00 02 00 00 00 0F 73 68 61 6B 65 5F 68 61 6E 64 5F 72 65 73 70 7F 7F'
        p = DBCPacket(msg)
        p.decode()
        print p

    def test_thrift_compact(self):
        # msg = '00 00 00 27 00 00 00 01 15 CF FD F2 E2 03 18 0E 73 68 61 6B 65 5F 68 61 6E 64 5F 72 65 71 18 05 31 32 33 34 35 7F 7F'
        msg = '00 00 00 24 00 00 00 01 15 82 02 18 0E 73 68 61 6B 65 5F 68 61 6E 64 5F 72 65 71 18 05 31 32 33 34 35 7F 7F'

        p = DBCPacket(msg)
        p.decode()
        print p

    def test_uncompress(self):
        # s = "30 2c 68 65 6c 6c 6f 20 77 6f 72 6c 64 0a 8e 0c 00"
        s = "0c 2c 68 65 6c 6c 6f 20 77 6f 72 6c 64 21"
        b = s_to_b(s)
        print snappy.uncompress(b)

        s = "00 00 08 3F 00 00 01 00 D1 2C F0 B2 08 00 01 E1 D1 A0 99 0B 00 02 00 00 00 15 73 65 72 76 69 63 65 5F 62 72 6F 61 64 63 61 73 74 5F 72 65 71 0B 00 03 00 00 00 32 32 34 44 77 39 6D 68 77 65 44 48 61 73 36 4B 59 61 76 67 52 66 67 45 68 50 6F 4C 31 70 45 70 39 47 46 37 6D 38 72 41 73 73 55 70 76 51 4B 64 50 76 6B 0B 00 04 00 00 00 00 7F 0D 00 01 0B 0C 00 00 00 20 00 00 00 2B 32 67 66 70 70 33 4D 41 42 33 76 68 33 33 6A 57 38 63 55 62 43 4B 41 6E 64 45 58 72 53 33 38 72 42 64 6D 37 4E 65 65 78 56 33 4B 0F 00 01 0B 00 00 00 01 00 00 00 0B 61 69 5F 74 72 61 69 6E 69 6E 67 09 AC 34 0C 31 30 2E 32 35 35 2E 30 2E 31 34 37 0A 05 A3 20 00 5B B0 BD BA 0D 00 04 0B 01 36 01 B4 4C 03 67 70 75 00 00 00 0D 32 20 2A 20 54 65 73 6C 61 50 34 30 01 85 30 05 73 74 61 74 65 00 00 00 04 69 64 6C 01 08 4C 07 76 65 72 73 69 6F 6E 00 00 00 07 30 2E 33 2E 34 2E 31 7F 36 AD 00 80 77 70 67 78 47 6D 39 65 77 61 50 50 75 58 4E 44 50 62 71 65 33 39 45 46 4A 6D 59 48 72 66 45 75 42 9E AD 00 04 31 33 19 AD 00 9F FE AD 00 5E AD 00 7C 79 4D 54 74 4B 4E 55 58 33 68 58 6B 41 45 74 65 34 48 42 31 31 79 7A 54 59 74 4C 72 6A 58 76 63 72 AD 00 04 0D 42 25 69 18 61 63 32 36 30 30 78 19 AE 00 B6 4A AE 00 00 15 21 5B 3C 47 65 46 6F 72 63 65 47 54 58 31 30 38 30 54 69 DA 63 01 80 78 34 38 66 73 42 64 78 6B 35 52 69 72 74 79 6F 35 35 47 4B 47 4D 70 6E 38 72 36 59 32 6E 35 39 71 72 B6 00 04 0B 31 51 10 00 32 5D 0F 00 AC 4A B4 00 04 0C 31 59 0F DE AB 00 78 68 59 31 77 39 31 52 4B 74 44 31 35 79 66 48 33 35 73 51 52 73 43 65 42 39 48 64 58 56 45 79 76 61 01 19 AB 00 38 5D 0D 00 AA FE AB 00 56 AB 00 80 7A 72 35 6E 66 51 6F 75 76 77 51 51 56 56 58 48 6B 54 57 6F 53 36 53 50 54 43 55 78 50 74 58 44 59 72 56 01 79 66 08 32 33 31 59 0B 00 AE FE AC 00 52 AC 00 84 34 31 57 67 4C 58 48 44 61 67 42 50 73 53 69 64 59 79 77 50 6B 36 6E 63 74 48 46 33 6B 57 57 66 62 5A 9E AC 00 04 30 36 19 AC 00 A6 4A AC 00 00 0D 5D 03 DA 12 04 84 34 32 50 56 37 79 4A 6A 79 67 36 6E 78 76 58 6E 66 55 51 74 4C 71 58 56 41 48 43 74 43 61 57 5A 54 67 72 AD 00 59 04 00 32 5D 04 00 B9 FE AC 00 5E AC 00 7C 54 57 7A 6A 55 77 48 44 45 56 4D 36 4D 67 36 47 58 6A 4B 69 39 44 48 4B 66 69 4E 45 36 39 5A 48 72 AC 00 18 06 6D 69 6C 75 6D 61 39 53 00 A4 4A A7 00 0C 15 34 20 2A FE 0A 04 8D 0A 84 34 32 71 58 7A 75 6D 58 6B 59 5A 35 34 75 53 72 50 53 74 65 74 50 34 31 63 79 6E 7A 38 74 57 6B 56 37 72 AF 00 04 08 6D C5 27 04 39 38 19 B1 00 B8 4E B1 00 00 38 FE B1 00 19 B1 80 33 42 4D 76 57 36 59 4C 47 78 79 78 6D 63 67 78 66 78 63 53 45 55 6B 75 43 65 51 4D 57 69 6A 7A 47 72 B1 00 59 0C 04 36 34 19 B4 4E BB 04 EE CA 06 CD CA 84 34 33 5A 57 48 35 66 63 4C 44 65 70 57 34 7A 31 58 53 45 32 72 6F 33 59 61 6E 36 59 76 36 57 65 44 51 72 AC 00 2E 11 04 3D 61 00 A2 4A 61 01 EE 68 05 A9 68 84 34 33 75 58 55 78 6A 47 75 77 59 51 4E 63 6F 4E 75 68 51 76 37 59 4E 78 71 38 65 5A 64 37 75 7A 6A 74 9A AC 00 00 31 2E 11 04 4E BE 02 FE 11 04 81 11 80 33 76 75 4A 37 6D 7A 5A 76 64 36 46 52 58 4C 67 78 74 57 38 6D 52 6A 34 50 79 33 31 6A 74 62 67 72 9E AD 00 00 38 DD C1 00 B4 4A 59 01 FE 06 02 41 06 80 34 71 7A 78 61 6D 74 73 4C 6B 59 43 4A 39 38 78 6A 4D 31 69 39 68 44 65 6E 6A 4E 51 7A 50 66 63 62 9E AD 00 00 39 5D B3 00 AD 4E AD 00 EE 6B 05 AD 6B 80 35 32 55 61 48 6A 31 42 68 4A 70 64 35 6E 58 38 78 61 62 61 69 73 63 78 35 73 6F 37 45 33 6F 52 31 72 AD 00 79 5F 2E B2 02 4E 5F 03 EE B2 02 4D B2 7C 35 34 6B 75 65 5A 4E 34 31 55 52 63 64 6E 6F 66 7A 58 4A 66 55 36 67 6B 34 4C 31 55 56 35 66 6D 76 D5 0A 24 0A 69 6E 73 74 61 6E 63 65 2D FD 6D 4E 56 01 00 18 0E 78 09 16 D3 0A 30 31 30 30 2D 50 43 49 45 2D 31 36 47 42 9A D0 08 00 30 36 DE 0A 80 34 35 6F 77 38 48 47 4B 31 71 79 51 4A 74 32 77 6E 45 77 72 7A 68 72 6F 5A 6F 33 51 6B 52 38 63 32 76 D0 08 38 0F 53 6C 6F 76 65 6E 69 61 38 47 50 55 2D 42 1D BB 4E 17 04 FE 78 05 2E 78 05 80 37 36 48 79 6B 53 72 68 64 77 35 50 54 57 69 6E 75 4E 56 55 6F 39 59 63 67 69 6E 6E 35 57 43 50 46 72 19 02 28 0A 6A 69 6D 6D 79 27 73 20 70 63 B9 77 00 A7 51 C4 08 05 00 00 1E 41 0C 00 11 41 C4 00 47 16 E6 0A 10 39 34 30 4D 58 21 67 20 09 67 70 75 5F 75 73 61 67 0E 41 0C 48 03 30 20 25 00 00 00 0C 73 74 61 72 74 75 70 5F 74 69 6D 01 17 30 0A 31 35 33 38 32 37 32 36 39 35 00 00 CA 77 0C 80 34 37 47 4A 35 6D 67 63 5A 32 73 73 73 33 6B 53 35 37 33 4D 74 32 66 62 35 56 59 58 62 77 79 52 65 76 4F 02 B9 AD 04 31 35 BD AD 00 B1 11 E3 2E 24 0D 01 E3 49 51 E2 12 09 7C 37 58 61 35 78 56 54 31 74 36 47 39 6B 57 37 67 6F 4A 43 31 61 78 37 66 58 70 5A 7A 78 72 53 46 76 BF 09 24 09 61 6E 6F 6E 79 6D 6F 75 73 39 8D 00 A5 4A AA 00 10 04 4E 2F 41 0A 96 4E 01 3E E7 02 80 38 6E 33 52 46 34 43 76 6E 36 4A 4A 41 39 32 68 4B 44 71 79 72 75 64 32 55 74 33 6A 6D 46 73 51 41 72 2F 02 4E A1 00 22 59 08 0E 3F 0F 1A 6F 0E 9A 0A 09 3A 83 03 80 41 36 32 71 65 51 48 6F 58 6F 42 68 44 6B 78 57 6A 67 79 41 6D 78 57 51 53 38 4D 5A 59 7A 32 59 4A 72 9C 00 99 E4 04 34 39 39 3F 4E 7F 03 EE E4 04 8D E4 7C 42 42 36 35 57 4C 66 5A 44 61 34 37 64 4D 48 71 68 70 54 6E 32 79 41 75 5A 54 57 77 71 7A 36 4C 76 47 01 00 0E 1E 5F 0E 0C 2D 58 39 39 2E E8 04 4E EB 06 9A 4C 01 0E B8 0F 7E C3 0F 80 34 43 54 38 54 72 34 68 61 77 68 79 69 68 31 34 66 57 79 38 72 67 76 57 72 39 66 38 65 6D 62 33 75 76 F9 08 20 08 66 75 63 6B 66 61 63 65 39 5F FE B1 00 82 B1 00 7C 67 61 52 42 36 73 4E 46 63 36 7A 76 59 73 51 59 77 46 37 69 79 36 6F 59 6D 65 68 76 6F 6B 37 75 72 13 02 79 FD 08 32 34 32 19 B5 4E 63 0C EE 14 02 4D 14 80 44 53 32 4B 63 5A 78 4B 47 42 61 73 64 74 66 56 46 69 36 34 77 62 36 70 33 75 37 39 32 50 34 37 79 9A AC 00 00 31 2E AC 00 00 B2 4A FF 03 FE AC 00 01 AC 78 63 57 4A 58 4C 69 70 36 52 34 73 39 4B 61 42 47 5A 4A 78 61 33 47 75 6E 6F 54 56 36 46 34 6E 76 A8 04 7D 6B 00 35 39 57 D5 37 2E 54 05 F9 A5 E2 54 05 7C 46 52 58 47 42 41 66 61 67 53 32 62 53 34 67 55 76 39 75 42 76 38 63 6B 76 68 69 36 4B 46 6F 77 76 B5 02 4E B3 04 00 9E 4A 55 01 FE C7 11 2A C7 11 84 34 46 68 34 6F 55 4C 76 51 52 7A 45 45 50 47 4A 6A 35 62 58 50 63 79 42 6B 39 6E 65 32 76 79 58 50 55 72 0A 02 39 5E 00 33 3D 5E 4E D6 13 EE 09 02 4D 09 80 4A 50 50 71 7A 46 53 31 71 51 4E 4C 31 36 53 41 41 32 4A 78 4A 59 7A 6B 4A 67 52 41 43 47 47 50 4C 72 AB 00 4E 5D 01 00 B5 4E 5D 01 FE 1A 0F 2A 1A 0F 7C 4A 78 79 75 41 55 61 48 52 46 6A 59 31 6B 76 59 4A 58 70 65 76 5A 68 46 41 53 53 57 63 6E 53 68 76 75 10 9D 13 00 32 2A A6 09 00 AD 4A B5 00 62 5E 01 1C 07 62 75 73 79 28 31 29 5A 73 05 00 7F"

        p = DBCPacket(s)
        p.decode()
        print p

    def test_start_training_req(self):

        s = "00 00 02 9A 00 00 00 00 08 00 01 E1 D1 A0 99 0B 00 02 00 00 00 12 73 74 61 72 74 5F 74 72 61 69 6E 69 6E 67 5F 72 65 71 0B 00 03 00 00 00 31 4E 46 6E 6F 5A 54 39 62 7A 61 43 36 6B 4C 61 6A 54 61 35 77 67 7A 51 38 4B 54 45 4C 52 62 7A 59 4C 6B 47 75 65 31 6B 36 6E 75 46 57 45 54 61 77 4D 0D 00 FF 0B 0B 00 00 00 03 00 00 00 09 6F 72 69 67 69 6E 5F 69 64 00 00 00 2B 32 67 66 70 70 33 4D 41 42 34 37 35 71 34 57 4E 79 79 75 4C 36 6B 50 55 6A 77 33 6E 65 64 4A 4C 4B 52 67 69 69 4A 69 55 6A 37 5A 00 00 00 04 73 69 67 6E 00 00 00 82 31 66 66 65 30 62 65 36 35 38 36 62 64 34 39 36 38 61 65 65 61 36 30 30 64 38 30 37 63 30 65 65 64 33 38 37 63 61 66 61 63 36 65 30 62 35 34 66 34 34 32 33 38 31 37 30 34 62 65 30 66 33 65 66 30 61 34 31 30 37 35 34 63 61 32 30 31 35 38 31 64 36 35 66 66 62 39 65 39 31 31 36 33 34 66 38 33 62 39 64 61 38 63 37 30 31 34 63 61 35 61 65 34 61 30 33 34 37 32 39 62 65 33 31 32 36 37 36 66 61 00 00 00 09 73 69 67 6E 5F 61 6C 67 6F 00 00 00 05 65 63 64 73 61 7F 0B 00 01 00 00 00 32 32 6E 6E 44 7A 31 7A 5A 70 6D 4C 72 6A 6D 75 74 71 45 45 6B 33 4B 57 32 51 68 57 57 44 73 53 53 37 62 72 51 79 66 44 46 51 54 55 34 72 55 69 61 62 51 03 00 02 00 0B 00 03 00 00 00 00 0F 00 04 0B 00 00 00 01 00 00 00 2B 32 67 66 70 70 33 4D 41 42 34 37 36 48 79 6B 53 72 68 64 77 35 50 54 57 69 6E 75 4E 56 55 6F 39 59 63 67 69 6E 6E 35 57 43 50 46 0B 00 05 00 00 00 00 08 00 06 00 00 00 00 0B 00 07 00 00 00 30 64 62 63 74 72 61 69 6E 69 6E 67 2F 74 65 6E 73 6F 72 66 6C 6F 77 31 2E 39 2E 30 2D 63 75 64 61 39 2D 67 70 75 2D 70 79 33 3A 76 31 2E 30 2E 30 0B 00 08 00 00 00 2E 51 6D 57 38 76 55 6D 48 72 68 74 33 73 59 39 34 56 47 36 65 4B 6F 4A 56 51 33 50 69 38 4D 4B 4C 62 43 62 71 75 59 4D 56 6E 42 66 61 33 7A 0B 00 09 00 00 00 06 72 75 6E 2E 73 68 0B 00 0A 00 00 00 00 0B 00 0B 00 00 00 00 0B 00 0C 00 00 00 00 0B 00 0D 00 00 00 31 44 44 6A 33 52 47 4B 53 6E 6A 61 51 47 68 78 37 56 66 77 68 7A 31 33 58 5A 34 64 58 34 48 6B 56 65 45 6F 51 61 32 78 54 6A 34 37 62 42 4E 75 39 64 7F"
        p = DBCPacket(s)
        p.decode()
        print p


if __name__=="__main__":
    msg = load_msg()

    if len(msg.strip()) == 0:
        exit(1)

    # msg = '08 00 01 E1 D1 A0 97 0B 00 02 00 00 00 0F 73 68 61 6B 65 5F 68 61 6E 64 5F 72 65 73 70 7F 7F'
    p = DBCPacket(msg)
    p.decode()
    print p
