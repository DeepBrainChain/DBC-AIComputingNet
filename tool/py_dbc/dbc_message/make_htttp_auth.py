import time
import urllib
import json
import base64
import json
from dbc_message.msg_common import *
def make_http_auth_req(mining_id,ai_user_id, task_id, task_state):
    auth_req={}
    auth_req["mining_node_id"]=mining_id
    auth_req["ai_user_node_id"]=ai_user_id
    auth_req["task_id"]=task_id
    auth_req["end_time"]=str(int(time.time()))
    auth_req["start_time"]=str(int(time.time()))
    auth_req["task_state"]=task_state
    message=auth_req["mining_node_id"]+auth_req["ai_user_node_id"]+auth_req["task_id"]+auth_req["start_time"]+auth_req["task_state"]+auth_req["end_time"]
    sign=dbc_sign(message)
    auth_req["sign_type"]="ecdsa"
    auth_req["sign"]=sign
    return auth_req
