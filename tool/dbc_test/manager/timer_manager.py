from service_manager import *
from common_def import service_name


class timer_manager(service_manager):
    def __init__(self):
        service_manager.__init__(self)

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