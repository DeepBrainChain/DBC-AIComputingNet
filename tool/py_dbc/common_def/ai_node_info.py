class ai_node_info:
    def __init__(self, node_id, version, gpu_model, gpu_num, time_stamp, service_list, name, state):
        self.node_id = node_id
        self.version = version
        self.gpu_num=gpu_num
        self.gpu_model=gpu_model
        self.state=state
        self.time_stamp=time_stamp
        self.service_list=service_list
        self.name=name
