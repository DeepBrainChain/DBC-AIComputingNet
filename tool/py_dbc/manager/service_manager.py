from threading import Thread
from common_def import service_name
# from queue import Queue
from Queue import Queue
from time import *
class service_manager(Thread):
    def  __init__(self):
        Thread.__init__(self)
        self.m_recvpool = Queue(1000)
        self.m_invokers = {}

    def service_init(self):
        pass

    def process(self):
        pass
    def service_stop(self):
        pass
    def srevice_name(self):
        return service_name.default_name

    def run(self):
        while True:
            if self.m_recvpool.not_empty:
                message = self.m_recvpool.get()
                if self.m_invokers.has_key(message.header.msg_name):
                    callback = self.m_invokers[message.header.msg_name]
                    callback(message)
                else:
                    print("m_invokers:", self.m_invokers)
                    print("invoker msg:", message.header.msg_name)
                    print("invoker error")
            sleep(0.1)

    def send(self, message):
        self.m_recvpool.put(message)