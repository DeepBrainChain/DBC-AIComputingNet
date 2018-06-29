#!/usr/bin/env python
import sys
import unittest
import types

from pprint import pprint
from matrix.ttypes import *
from matrix.ttypes_header import *
from thrift.protocol.TBinaryProtocol import TBinaryProtocol
from thrift.transport.TTransport import TMemoryBuffer
#import matrix.ttypes_header
#import ttypes_header


# redefine STOP mark from 0x00 to 0x7f
TType.STOP = 0x7f


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

    return len_, ''.join(bs)


# use globals() instead
# name_2_body ={
#     "shake_hand_req": empty,
#     "shake_hand_resp": empty,
#     "ver_req": ver_req_body,
#     "ver_resp": ver_resp_body,
#     "get_peer_nodes_req":  empty,
#     "get_peer_nodes_resp": get_peer_nodes_resp_body,
#     "peer_nodes_broadcast_req": peer_nodes_broadcast_req_body,
#     "start_training_req": start_training_req_body,
#     "stop_training_req": stop_training_req_body,
#     "list_training_req": list_training_req_body,
#     "list_training_resp": list_training_resp_body,
#     "logs_req": logs_req_body,
#     "logs_resp": logs_resp_body,
#     "show_req": show_req_body,
#     "show_resp": show_resp_body,
#     "service_broadcast_req": service_broadcast_req_body
# }


def decode_packet_header(p):
    # 4 bytes: len
    # 4 bytes: protocol
    return p.readI32(), p.readI32()


def decode(msg):
    msg_len, bs = s_to_b(msg)

    m = TMemoryBuffer(bs)
    p = TBinaryProtocol(m)

    packet_header_len, protocol_type = decode_packet_header(p)
    if packet_header_len != msg_len:
        print
        print "ERROR: packet_header.len is %d, but the input msg has %d bytes!" % (packet_header_len, msg_len)
        print
        return

    if protocol_type != 0:
        print "unknonw protocol type (%d)" % protocol_type
        return

    print
    print "packet header: \n{   'len': %d,\n    'protocol_type': %d\n}" % (packet_header_len, protocol_type)

    try:
        # h = msg_header()
        h = msg_header()
        h.read(p)
    except EOFError:
        print "Error: msg header decode failure"
        return

    print "msg header: "
    pprint(vars(h), indent=4)

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

    print "body: "
    pprint(vars(body), indent=4, width=24)
    print


def decode2(msg,msg_len):
    print "#############################################"
    m = TMemoryBuffer(msg)
    p = TBinaryProtocol(m)


    packet_header_len, protocol_type = decode_packet_header(p)
    if packet_header_len != msg_len:
        print
        print "ERROR: packet_header.len is %d, but the input msg has %d bytes!" % (packet_header_len, msg_len)
        print
        return

    if protocol_type != 0:
        print "unknonw protocol type (%d)" % protocol_type
        return

    print
    print "packet header: \n{   'len': %d,\n    'protocol_type': %d\n}" % (packet_header_len, protocol_type)
    print "---------------------------------------------------"
    try:
        # h = msg_header()
        h = msg_header()
        h.read(p)
    except EOFError:
        print "Error: msg header decode failure"
        return

    print "msg header: "
    pprint(vars(h), indent=4)
    print "---------------------------------------------------"
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

    print "body: "
    pprint(vars(body), indent=4, width=24)
    print "#############################################"
    return packet_header_len, protocol_type,h,body



class TestDecode(unittest.TestCase):

    def test_handshake(self):
        msg = '00 00 00 27 00 00 00 00 08 00 01 E1 D1 A0 97 0B 00 02 00 00 00 0F 73 68 61 6B 65 5F 68 61 6E 64 5F 72 65 73 70 7F 7F'
        decode(msg)


    def test_get_peer_nodes_rsp(self):
        msg = '00 00 00 E3 00 00 00 00' \
              ' 08 00 01 E1 D1 A0 97 0B 00 02 00 00 00 13 67 65 74 5F 70 65 65 72 5F 6E 6F 64 65 73 5F 72 65 73 70' \
              ' 0B 00 03 00 00 00 31 44 68 7A 61 32 43 4C 52 55 47 6D 65 54 48 4B 46 6B 56 71 47 33 61 56 54 68 59' \
              ' 38 72 64 36 53 74 50 45 64 31 53 50 64 74 45 42 4B 68 70 53 42 52 34 7F' \
              ' 0F 00 01 0C 00 00 00 01' \
              ' 0B 00 01 00 00 00 2B 32 67 66 70 70 33 4D 41 42 34 42 38 58 41 38 66 65 56 56 77 4B 51 50 31 78 48 59 75 53 6A 72 61 34 42 59 42 38 73 33 45 58 74 31' \
              ' 08 00 02 00 00 00 00' \
              ' 08 00 03 00 00 00 00' \
              ' 08 00 04 00 00 00 00' \
              ' 0C 00 05' \
              ' 0B 00 01 00 00 00 09 31 32 37 2E 30 2E 30 2E 31' \
              ' 06 00 02 52 73' \
              ' 7F' \
              ' 0F 00 06 0B 00 00 00 01' \
              ' 00 00 00 0B 61 69 5F 74 72 61 69 6E 69 6E 67' \
              ' 7F' \
              ' 7F'
        decode(msg)


    def test_service_broadcast_req(self):
        msg = '00 00 00 B6 00 00 00 00' \
              ' 08 00 01 E1 D1 A0 97 0B 00 02 00 00 00 15 73 65 72 76 69 63 65 5F 62 72 6F 61 64 63 61 73 74 5F 72 65 71' \
              ' 0B 00 03 00 00 00 31 7A 31 43 47 4C 72 37 53 4E 79 6D 4D 4C 66 50 72 54 6B 79 51 71 79 4A 43 45 50 64 48 33 47 32 70 6B 77 63 45 79 56 70 78 53 4E 76 34 51 50 32 43 78 0B 00 04 00 00 00 03 31 32 33 7F' \
              ' 0D 00 01' \
              ' 0B 0C 00 00 00 01 00 00 00 2B' \
              ' 32 67 66 70 70 33 4D 41 42 34 41 45 72 4E 31 62 64 7A 43 63 71 72 4B 61 4D 78 56 4B 79 48 45 73 74 58 54 52 78 45 72 39 55 66 75' \
              ' 0F 00 01 0B 00 00 00 01 00 00 00 02 61 69 7F 7F'

        decode(msg)


if __name__=="__main__":
    msg = load_msg()

    if len(msg.strip()) == 0:
        exit(1)

    print
    print "msg: "+ msg

    # msg = '08 00 01 E1 D1 A0 97 0B 00 02 00 00 00 0F 73 68 61 6B 65 5F 68 61 6E 64 5F 72 65 73 70 7F 7F'
    decode(msg)
