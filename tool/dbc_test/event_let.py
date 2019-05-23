#!/usr/bin/env python

import eventlet
from network import tcp_client


def dbc_handler(dbc_client_list):
    for client in dbc_client_list:
        print(client.m_s.getsockname())
        client.out_data(make_ver_req(HOST,PORT,gen_node_id()))
        client.send()

from dbc_message.node_ver_req import  *
HOST="10.10.254.187"
PORT=21107
clientlist= []

client = tcp_client.dbc_tcp_client((HOST, PORT))
client.connect()

client2 = tcp_client.dbc_tcp_client((HOST, PORT))
client2.connect()

clientlist.append(client)
clientlist.append(client2)

pool = eventlet.GreenPool(2)
pool.spawn_n(dbc_handler, clientlist)
# pool.running()
pool.waitall()
# client.connect()
# localip,localport= client.m_s.getsockname()
# print(localip,localport)
# client.PutInData(make_ver_req(HOST,PORT,gen_node_id()))
# client.SendData()