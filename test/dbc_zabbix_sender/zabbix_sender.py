#!/usr/bin/python
#-*- coding:utf8 -*-
__author__ = 'pdd'
__date__ = '2016/11/28'

''' script simulate zabbix_sender '''

import sys
import json
import time
import struct
import socket
import argparse

parser = argparse.ArgumentParser(description='script simulate zabbix_sender')
parser.add_argument('-z','--server',dest='server',action='store',help='Zabbix server ip')
parser.add_argument('-p','--port',dest='port',action='store',help='Zabbix server port',default=10051,type=int)
parser.add_argument('-s','--host',dest='host',action='store')
parser.add_argument('-k','--key',dest='key',action='store',help='item key')
parser.add_argument('-o','--value',dest='value',action='store',help='item value')
args = parser.parse_args()

class Metric(object):
    def __init__(self, host, key, value):
        self.host = host
        self.key = key
        self.value = value

    def __repr__(self):
        result = 'Metric(%r, %r, %r)' % (self.host, self.key, self.value)
        return result

def send_to_zabbix():
    j = json.dumps
    m = Metric(args.host, args.key, args.value)
    clock = ('%d' % time.time())
    metrics = '{"host":%s,"key":%s,"value":%s,"clock":%s}' % (j(m.host), j(m.key), j(m.value), j(clock))
    json_data = '{"request":"sender data","data":[%s]}' % metrics
    data_len = struct.pack('<Q', len(json_data))
    packet = 'ZBXD\x01' + data_len + json_data
    try:
        zabbix = socket.socket()
        zabbix.connect((args.server, args.port))
        zabbix.sendall(packet)
        resp_hdr = zabbix.recv(13)
        resp_body_len = struct.unpack('<Q', resp_hdr[5:])[0]
        resp_body = zabbix.recv(resp_body_len)
        zabbix.close()
        resp = json.loads(resp_body)
        print(resp)
    except:
        print('Error while sending data to Zabbix')

if __name__=='__main__':
    send_to_zabbix()