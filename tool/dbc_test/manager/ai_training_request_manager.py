from service_manager import *


class ai_training_req_manager(service_manager):
    def __init__(self):
        service_manager.__init__(self)

    def start_train_req(self, msg):
        pass

    def list_task_req(self,msg):
        pass

    def log_req(self, msg):
        pass

    def stop_req(self, msg):
        pass

    def stop_req(self, msg):
        pass
    def srevice_name(self):
        return service_name.ai_power_requestor_service_name
    def run(self):
        print(self.srevice_name(), threading.currentThread().ident)
        # while True:
        #     print("aa")