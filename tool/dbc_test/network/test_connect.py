import tcp_client
from dbc_message.node_ver_req import *
import mt

def print_data(data):
    print("hello",data)
    mt.decode2(data, len(data))


connector = tcp_client.dbc_tcp_client(("114.116.19.45",21107), print_data)
ret = connector.connect()
connector.out_data(make_ver_req("114.116.19.45",21107, gen_node_id()))
while True:
    connector.process()