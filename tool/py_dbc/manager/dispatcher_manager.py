# from queue import Queue
from Queue import Queue
from manager.service_manager import *

import threading
from time import *
class dispatcher_manager(service_manager):
    def  __init__(self):
        service_manager.__init__(self)

        self.remote_call = {}

    def service_init(self):
        pass

    def process(self):
        print(self.srevice_name(), threading.currentThread().ident)
        # while True:
        #     print("data")

    def service_stop(self):
        pass

    def subcribe(self, topic, callback):
        self.remote_call[topic] = callback

    def publish(self, topic, message):
        callback = self.remote_call[topic]
        callback(message)

    def run(self):
        while 1:
            # self.process()
            # print(self.srevice_name(), threading.currentThread().ident, ctime())
            sleep(1)


    def srevice_name(self):
        return service_name.topic_manager_name


    def put_in(self, in_data):
        pass
