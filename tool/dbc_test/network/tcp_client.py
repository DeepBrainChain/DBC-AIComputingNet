import os
import socket
from time import sleep
from Queue import Queue
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
        except socket.error, msg:
            print("Socket connect error! %s" %(msg))
            return -1
    def process (self):
        if self.m_IsConnect != 1:
            sleep(1)
            return -1
        else:
            infds, outfds, errfds = select.select([self.m_s,],[self.m_s,],[self.m_s,],1)
            if len(infds)!=0:
                if self.recv() != 0:
                    self.m_IsConnect = 0
                    return -1
            elif len(outfds)!=0:
                if self.send() != 0:
                    self.m_IsConnect = 0
                    return -1
            else:
                self.m_IsConnect = 0
                return -1
            sleep(1)
        return 0

    def out_data (self,InData):
        if self.m_sendpool.full():
            return -1
        self.m_sendpool.put(InData)
        return 0

    def send (self):
        if self.m_sendpool.empty():
            return 0
        data = self.m_sendpool.get()
        try:
            self.m_s.send(data)
            return 0
        except socket.error, msg:
            print("Socket send error! %s" %(msg))
            return -1

    def recv (self):
        try:
            data = self.m_s.recv(BUFSIZ)
            if self.callbak(data) !=0 :
                return -1
            return 0
        except socket.error, msg:
            print("Socket send error! %s" %(msg))
            return -1

    def state(self):
        return self.m_IsConnect

