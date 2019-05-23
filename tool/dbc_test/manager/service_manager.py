from threading import Thread
from common_def import service_name


class service_manager(Thread):
    def  __init__(self):
        Thread.__init__(self)
    def service_init(self):
        pass
    def process(self):
        pass
    def service_stop(self):
        pass
    def srevice_name(self):
        return service_name.default_name
    def run(self):
        pass