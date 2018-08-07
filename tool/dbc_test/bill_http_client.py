import time
import BaseHTTPServer
import urllib
import json
import base64
import json
import eventlet

from threading import Thread,currentThread,activeCount,Lock
import threading

import requests
from dbc_message.make_htttp_auth import *

mutex=threading.Lock()
bill_url="https://info.deepbrainchain.org/open-api/chain/auth-task"
gen_node_id()
headers = \
{
    "Host":"info.deepbrainchain.org",
    "Content-Type": "application/json;charset=UTF-8"
}


msg_count=0
def send_auth():
    ai_user="2gfpp3MAB3ygccXrgc8Tk5dfCZicxzzQHN17PE4fT2C"
    task_id="2MfHLS3yaeb6u4DKAd4maDcLRPhboD2j6imz7KPB2pekLcRCNa"
    task_state="task_running"

    while True:
        auth_req = make_http_auth_req(get_node_id(),ai_user,task_id,task_state)
        mm=json.dumps(auth_req)
        req_t=time.time()
        ret=requests.post(bill_url,headers=headers, data=mm)
        # mutex.acquire()
        global msg_count
        msg_count =  msg_count +1
        rsp_t = time.time()
        # print(rsp_t)
        inter_val= rsp_t - req_t
        print("send message:", msg_count)
        print(inter_val, "     ", ret.json())
        # mutex.release()

i=0
print("begin:",time.time())
while i<5000:
    # pool.spawn_n(send_auth)
    Thread(target=send_auth).start()
    i=i+1

