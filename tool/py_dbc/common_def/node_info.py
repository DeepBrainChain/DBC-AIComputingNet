class node_info:
    def __init__(self, node_id, version, sid, start, addr):
        self.node_id = node_id
        self.version = version
        self.m_sid = sid
        self.start_time = start
        self.nLastSend = 0
        self.nLastRecv = 0
        self.nSendBytes = 0
        self.nRecvBytes = 0
        self.nTimeOffset = 0
        self.fSuccessfullyConnected = 0
        self.fDisconnect = 0
        self.host=addr[0]
        self.port=addr[1]