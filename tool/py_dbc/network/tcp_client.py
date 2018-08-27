import os
import socket
from time import sleep
# from queue import Queue
from Queue import Queue
# from queue import LifoQueue
from Queue import LifoQueue
from plistlib import *

import select
import struct
import binascii

BUFSIZ=10240
class tcp_client:
    def __init__ (self, address, callbak):
    # def __init__(self, address):
        self.address = address
        self.m_s     = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.m_recvpool = Queue(1000)
        self.m_sendpool = Queue(1000)
        self.m_IsConnect = 0
        self.callbak = callbak

    def connect (self):
        try:
            # print('host is address'+self.address)
            ret = self.m_s.connect(self.address)
            self.m_IsConnect = 1
            return 0
        except socket.error:
            print("Socket connect error! " ,socket.error)
            return -1
    def process (self):
        if self.m_IsConnect != 1:
            sleep(1)
            return -1
        else:
            infds, outfds, errfds = select.select([self.m_s,],[self.m_s,],[self.m_s,],1)
            if len(infds)!=0:
                if self.recv() != 0:
                    print("recv error")
                    self.m_IsConnect = 0
                    return -1
            elif len(outfds)!=0:
                if self.send() != 0:
                    print("send error")
                    self.m_IsConnect = 0
                    return -1
            else:
                print("error error")
                self.m_IsConnect = 0
                return -1
            # sleep(1)
        return 0

    def put (self,InData):
        if self.m_IsConnect != 1:
            print("put.channel disconnect")
            return -1
        if self.m_sendpool.full():
            print("send.pool full")
            return -1
        self.m_sendpool.put(InData)
        return 0

    def send (self):
        if self.m_IsConnect != 1:
            print("send error")
            return -1
        if self.m_sendpool.empty():
            # print("send empty")
            return 0
        data = self.m_sendpool.get()
        try:
            print("send:", binascii.hexlify(data))
            self.m_s.send(data)
            return 0
        except socket.error:
            print("Socket send error!",socket.error)
            return -1

    def recv (self):
        if self.m_IsConnect != 1:
            print("recv disconnect")
            return -1
        try:
            data = self.m_s.recv(BUFSIZ)
            if self.callbak(data) !=0 :
                print("recv call back error")
                return -1
            return 0
        except socket.error:
            print("Socket send error!",socket.error)
            return -1

    def state(self):
        return self.m_IsConnect

