import threading
from  common_def.global_def import *
from common_def.service_name import *
from dbc_message.msg_common import *
from common_def.ai_node_info import *
from manager.service_manager import *
class ai_mining_device_manager(service_manager):
    def __init__(self):
        service_manager.__init__(self)
        self.m_gpu_models=set([])
        self.ai_node_maps={}

    def service_init(self):
        self.subscribe_init()
        self.invoker_init()
    def get_ai_mining_devices(self):
        return self.ai_node_maps.values()
    def get_gpu_models(self):
        return self.m_gpu_models;

    def get_ai_mining_by_condition(self,gpu_num,gpu_model):
        ai_devices=[]
        pos = 0
        for device in self.ai_node_maps.values():
            for model in device.gpu_model:
                if model == gpu_model and device.gpu_num[pos] >= gpu_num:
                    ai_devices.append(device)
            # if device.gpu_num >= gpu_num and device.gpu_model == gpu_model:
            #     ai_devices.append(device)
        return ai_devices

    def subscribe_init(self):
        get_manager(topic_manager_name).subcribe(SERVICE_BROADCAST_REQ, self.send)
        get_manager(topic_manager_name).subcribe(SHOW_RESP, self.send)
        get_manager(topic_manager_name).subcribe(SHOW_REQ, self.send)

    def invoker_init(self):
        self.m_invokers[SERVICE_BROADCAST_REQ]=self.on_service_broadcast_req
        self.m_invokers[SHOW_RESP] = self.on_show_resp
        self.m_invokers[SHOW_REQ] = self.on_show_req

    def on_service_broadcast_req(self, message):
        body = message.msg_content
        for node_id,node_info  in body.node_service_info_map.items():
            # node_id = key
            # node_info = body.node_service_info_map[node_id]
            name=node_info.name
            service_list=node_info.service_list
            time_stamp=node_info.time_stamp
            state=node_info.kvs["state"]
            version=node_info.kvs["version"] if node_info.kvs.has_key("version")  else "N/A"
            gpu_info=node_info.kvs["gpu"]
            gpu_nums = []
            gpu_models = []
            if  "N/A" in node_info.kvs["gpu"]:
                pass
            else:
                src_nodeinfo=node_info.kvs["gpu"].replace(" * ", "*").strip()
                gpu_infos=src_nodeinfo.split(" ")
                for gpu_info in gpu_infos:
                    gpu_num = int(gpu_info.split("*")[0])
                    gpu_model = str(gpu_info.split("*")[1])
                    self.m_gpu_models.add(gpu_model)
                    gpu_nums.append(gpu_num)
                    gpu_models.append(gpu_model)
                    self.m_gpu_models.add(gpu_model)
            if gpu_nums.__len__() < 1:
               gpu_nums.append(0)
            if gpu_models.__len__() < 1:
                gpu_models.append("N/A")
            ai_node = ai_node_info(node_id, version, gpu_models, gpu_nums, time_stamp, service_list, name, state)
            self.ai_node_maps[node_id] = ai_node
    def on_show_resp(self, message):
        pass

    def on_show_req(self, message):
        pass

    def srevice_name(self):
        return service_name.ai_mining_device_manager_name
