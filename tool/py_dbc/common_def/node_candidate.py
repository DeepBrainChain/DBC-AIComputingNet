class node_candidate(object):
    def __init__(self, addr, node_id="N/A",net_state="idle"):
        self.end_point=addr
        self.net_state =net_state
        self.reconn_cnt = 0
        self.last_connect_time = 0
        self.score = 0
        self.node_id = node_id
        self.core=""
        self.protocol=""