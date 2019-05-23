import os
import socket
from time import sleep
from Queue import Queue
from Queue import LifoQueue
from plistlib import *
from select import select
from threading import Thread
#from cmpp import *
from threading import Thread
#from account import *
import select
from matrix import *
from matrix.net_message import net_message
import struct
import binascii
BUFSIZ=10240

class TcpClient (Thread):
    def __init__ (self, PeerIPAddr, PeerPort):
        Thread.__init__(self)
        self.m_host = PeerIPAddr
        self.m_port = PeerPort
        self.m_s    = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.m_recvpool = Queue(1000)
        self.m_sendpool = Queue(1000)
        self.m_IsConnect = 0
        self.net_msg = net_message()


    def setPeerIP (self,PeerIPAddr):
        self.host = PeerIPAddr
    def setPeerPort (self,PeerPort):
        self.port = PeerPort
    def connect (self):
        try:
            print('host is %s, port is %d' %(self.m_host,self.m_port))
            ret = self.m_s.connect((self.m_host, self.m_port))
            self.m_IsConnect = 1
            return 0
        except socket.error, msg:
            print("Socket connect error! %s" %(msg))
            return -1
    def close(self):
        try:
            print('host is %s, port is %d' %(self.m_host,self.m_port))
            ret = self.m_s.close()
            self.m_IsConnect = 0
            self.join()
            return 0
        except socket.error, msg:
            print("Socket connect error! %s" %(msg))
            return -1
    def run (self):
        try:
            while 1:
                if self.m_IsConnect != 1:
                    sleep(1)
                    continue
                else:
                    while 1:
                        infds, outfds, errfds = select.select([self.m_s,],[self.m_s,],[self.m_s,],1)
                        if len(infds)!=0:
                            if self.RecvData() != 0:
                                #self.m_IsConnect = 0
                                break
                        elif len(outfds)!=0:
                            if self.SendData() != 0:
                                #self.m_IsConnect = 0
                                break
                        else:
                            self.m_IsConnect = 0
                            break
                        # sleep(1)
        except socket.error, msg:
            print("Socket connect error! %s" %(msg))
            return -1

    def PutInData (self,InData):
        #print "bef put, q size:"
        #print self.m_sendpool.qsize()
        if self.m_sendpool.full():
            return -1
        self.m_sendpool.put(InData)
        #print "after put, q size:"
        #print self.m_sendpool.qsize()
        return 0

    def SendData (self):
        if self.m_sendpool.empty():
            return 0

        #print "q size:"
        #print self.m_sendpool.qsize()
        data = self.m_sendpool.get()
        #print "after get"
        #print self.m_sendpool.qsize()
        try:
            print("send:",binascii.hexlify(data))
            self.m_s.send(data)
            return 0
        except socket.error, msg:
            print("Socket send error! %s" %(msg))
            return -1


    def pre_deal_data(self, in_data):
        if (len(in_data) < 1):
            return
        off_set = 0
        src_len=len(in_data)
        while off_set <= src_len:
            if self.net_msg.lenth < 1:
                self.net_msg.lenth = struct.unpack('!L', in_data[0:4])[0]
            if self.net_msg.lenth > 102400:
                return -1

            copy_len = self.net_msg.lenth - len(self.net_msg.data)
            if copy_len==0:
                self.m_recvpool.put(self.net_msg)
                self.net_msg = net_message()
                copy_len = self.net_msg.lenth - len(self.net_msg.data)
            remain_len = src_len - off_set
            if remain_len==0:
                return 0
            copy_len = min(copy_len, remain_len)
            if copy_len > 0 :
                self.net_msg.data = self.net_msg.data  + in_data[off_set:copy_len]
                print(self.net_msg.data)
                off_set += copy_len
        return 0

    def RecvData (self):
        if self.m_recvpool.full():
            return 0
        try:
            data = self.m_s.recv(BUFSIZ)
            if self.pre_deal_data(data) !=0 :
                return -1
            # self.net_msg.add_data(data)
            # self.m_recvpool.put(data)
            return 0
        except socket.error, msg:
            print("Socket send error! %s" %(msg))
            return -1

    def GetData (self):
        if self.m_recvpool.empty():
            return []
        net_msg = self.m_recvpool.get()
        data = net_msg.data
        return data
        #return(self.m_recvpool.get())
    def IsHaveData (self):
        if self.m_recvpool.empty():
            return 0
        return 1