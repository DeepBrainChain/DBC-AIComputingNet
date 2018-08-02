#!/usr/bin/env python
# -*- coding: utf-8 -*-

from scapy.all import *
import os
import time


def get_sip():
    return "10.10.254.198"
#generate normal packet: packet type is syn
def gen_np_syn(dst,dport,sport=20,seq=11111):
    ip=IP(dst=dst,src=get_sip())
    tcp=TCP(sport=sport,dport=dport,seq=seq,flags='S')
    #hexdump(tcp)
    p = ip/tcp
    p.display()
    return p
#generate normal packet: packet type is push+ack
def gen_np_pushack(dst,dport,seq ,ack ,sport=20):
    ip=IP(dst=dst,src=get_sip())
    #tcp=TCP(sport=sport,dport=dport,seq=seq,ack=ack,flags='PA',chksum=90)
    tcp=TCP(sport=sport,dport=dport,seq=seq,ack=ack,flags='PA')
    data="GET /  HTTP/1.1\r\nUser-Agent: Microsoft-ATL-Native/8.00\r\n\r\n"
    p = ip/tcp/data
    p.display()
    return p
#generate normal packet: packet type is ack
def gen_np_ack(dst,dport,seq ,ack ,sport=20):
    ip=IP(dst=dst,src=get_sip())
    tcp=TCP(sport=sport, dport=dport,seq=seq, ack=ack,flags='A')
    p = ip/tcp
    p.show()
    return p
#generate normal packet: packet type is fin
def gen_np_fin(dst,dport,seq ,ack ,sport=20):
    ip=IP(dst=dst,src=get_sip())
    tcp=TCP(sport=sport,dport=dport,seq=seq,ack=ack,flags='FA')
    p = ip/tcp
    p.show()
    return p

def reuse(push=True,fin=False):
    #dip='123.123.167.100'
    #dip='192.168.91.6'
    dip='10.10.254.187'
    dport=21107
    #sport=random.randint(10000,60000)
    sport=28274
    seq = random.randint(10000,60000)
    os.popen('iptables  -A OUTPUT  -p tcp --dport %d --tcp-flag RST RST --dst %s -j DROP' %(dport, dip))
    np_s=gen_np_syn(dip,dport,sport,seq)
    res_sa = sr1(np_s)
    res_sa.display()
    if push:
        np_pa=gen_np_pushack(dip,dport,res_sa.ack,res_sa.seq+1,sport)
    else:
        np_pa=gen_np_ack(dip,dport,res_sa.ack,res_sa.seq+1,sport)
    res_pa = send(np_pa)
    if fin:
        np_fa=gen_np_fin(dip,dport,res_sa.ack,res_sa.seq+1,sport)
        send(np_fa)
    os.popen('iptables  -D OUTPUT  -p tcp --dport %d --tcp-flag RST RST --dst %s -j DROP' %(dport, dip))


def main():
    #reuse(False,True)
    #time.sleep(1)
    sport = random.randint(1024, 65535)
    dst="10.10.254.187"
    dst_port=21107
    reuse()
    # (dst,dport,sport=20,seq=11111):
    #gen_np_syn(dst, dst_port, sport)
    # i = 0
    # while i < 20:
    #     i += 1
    #     reuse(True,True)
    #     time.sleep(1)
if __name__ == "__main__":
    main()
