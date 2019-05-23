class node_info:
    def __init__(self, in_socket, address, sid, start):
        self.m_socket = in_socket
        self.m_address = address
        self.m_sid = sid
        self.start_time = start
        self.nLastSend = 0
        self.nLastRecv = 0
        self.nSendBytes = 0
        self.nRecvBytes = 0
        self.nTimeOffset = 0
        self.fSuccessfullyConnected = 0
        self.fDisconnect = 0