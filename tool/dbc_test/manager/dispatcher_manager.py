from Queue import Queue
from service_manager import *
from matrix.net_message import *
class dispatcher_manager(service_manager):
    def  __init__(self):
        service_manager.__init__(self)
        self.m_recvpool = Queue(1000)
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

    def run(self):
        while 1:
            # self.process()
            print(self.srevice_name(), threading.currentThread().ident, ctime())
            sleep(1)

    def srevice_name(self):
        return service_name.topic_manager_name

    def pre_deal_data(self, in_data):
        if (len(in_data) < 1):
            return
        off_set = 0
        src_len=len(in_data)
        while off_set <= src_len:
            if self.net_msg.lenth < 1:
                self.net_msg.lenth = struct.unpack('!L', in_data[0:4])[0]
            if self.net_msg.lenth > 102400:
                return -1

            copy_len = self.net_msg.lenth - len(self.net_msg.data)
            if copy_len==0:
                self.m_recvpool.put(self.net_msg)
                self.net_msg = net_message()
                copy_len = self.net_msg.lenth - len(self.net_msg.data)
            remain_len = src_len - off_set
            if remain_len==0:
                return 0
            copy_len = min(copy_len, remain_len)
            if copy_len > 0 :
                self.net_msg.data = self.net_msg.data  + in_data[off_set:copy_len]
                print(self.net_msg.data)
                off_set += copy_len
        return 0

    def put_in(self, in_data):
        ret = self.pre_deal_data(in_data)
        return ret
