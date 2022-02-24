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
parser.add_argument('-s','--server',dest='server',action='store',help='Zabbix server ip',default='116.169.53.132')
parser.add_argument('-p','--port',dest='port',action='store',help='Zabbix server port',default=10051,type=int)
parser.add_argument('-n','--host',dest='host',action='store',help='hostname',default='14.152.85.57')
args = parser.parse_args()

def send_to_zabbix():
    j = json.dumps
    json_data = '{"request":"active checks","host":"%s"}' % args.host
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