import threading
from  common_def.global_def import *
from common_def.service_name import *
from dbc_message.msg_common import *
from manager.service_manager import *
from dbc_message.node_cmd_start_training_req import *
import ipfsapi
from  training_task_def.task_state import *
from training_task_def.formte_task_state import *
from time import time
import os
import commands
class ai_training_req_manager(service_manager):
    def __init__(self):
        service_manager.__init__(self)

    def service_init(self):
        self.subscribe_init()
        self.invoker_init()

    def subscribe_init(self):
        get_manager(topic_manager_name).subcribe(AI_TRAINING_NOTIFICATION_REQ, self.send)
        get_manager(topic_manager_name).subcribe(STOP_TRAINING_REQ, self.send)
        get_manager(topic_manager_name).subcribe(LIST_TRAINING_REQ, self.send)
        get_manager(topic_manager_name).subcribe(LIST_TRAINING_RESP, self.send)
        get_manager(topic_manager_name).subcribe(LOGS_REQ, self.send)
        get_manager(topic_manager_name).subcribe(LOGS_RESP, self.send)

    def invoker_init(self):
        self.m_invokers[AI_TRAINING_NOTIFICATION_REQ] = self.on_start_train_req
        self.m_invokers[STOP_TRAINING_REQ]=self.on_stop_train_req
        self.m_invokers[LIST_TRAINING_REQ] = self.on_list_task_req
        self.m_invokers[LIST_TRAINING_RESP] = self.on_list_task_resp
        self.m_invokers[LOGS_REQ] = self.on_log_req
        self.m_invokers[LOGS_RESP] = self.on_log_resp

    def cmd_start_train_req(self, code_dir, peer_nodes, entry_file, engine, hyper_params,code_dir_hash):
        code_hash = code_dir
        if code_dir_hash == False:
            api = ipfsapi.connect('127.0.0.1', 5001)
            res = api.add(code_dir,True)
            root_resp=res.pop()
            code_hash = root_resp["Hash"]
            cmd = 'curl "http://114.116.19.45:8080/ipfs/%s"' % (code_hash)
            (status, output) = commands.getstatusoutput(cmd)
            if status == 0:
                print("curl training_result_file_ipfspath success")
        # code_hash, entry_file, engine, peer_nodes, hyper_params, data_hash = ""
        task_id, message = make_start_training_req(code_hash,entry_file,engine,peer_nodes,hyper_params)
        get_manager(connection_manager_name).broad_cast(message)
        task = task_state(task_id, formate_task_state(1), int(time()), code_hash, engine,entry_file)
        return task

    def on_start_train_req(self, msg):
        pass
    def on_stop_train_req(self, msg):
        pass

    def on_list_task_req(self,msg):
        pass
    def on_list_task_resp(self,msg):
        pass

    def on_log_req(self, msg):
        pass
    def on_log_resp(self, msg):
        pass

    def srevice_name(self):
        return service_name.ai_power_requestor_service_name
    # def run(self):
    #     sleep(1)
    #     # print(self.srevice_name(), threading.currentThread().ident)
    #     # while True:
    #     #     print("aa")