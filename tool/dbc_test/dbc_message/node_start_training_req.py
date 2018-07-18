from matrix.ttypes import *
from matrix.ttypes_header import *
from matrix.dbc_service import *
from thrift.protocol.TBinaryProtocol import TBinaryProtocol
from thrift.transport.TTransport import TMemoryBuffer
from socket import *
import time
from msg_common import *
import binascii

def make_start_training_req(task_path):
    try:
        ifd = open(task_path, 'r')
        result = {}
        print("open file", task_path, "fd is", ifd)
        for strline in ifd.readlines():
            # print(strline)
            if not len(strline):
                continue
            temp_store = strline.split('=');
            result[temp_store[0]] = temp_store[1].strip("\n").strip("\r")
        ifd.close()

        m = TMemoryBuffer()
        p = TBinaryProtocol(m)
        msg_name = AI_TRAINING_NOTIFICATION_REQ
        nonce = get_random_id()
        head = msg_header(magic, msg_name, nonce)
        head.write(p)
        task_id = get_random_id()
        print "task id"
        print task_id
        # select_mode=bytes(result["select_mode"])[0]
        select_mode = 0x00
        master = result["master"]
        pns = result["peer_nodes_list"]
        peer_nodes_list = pns.split(",")
        server_specification = result["server_specification"]
        server_count = int(result["server_count"])
        training_engine = result["training_engine"]
        code_dir = result["code_dir"]
        entry_file = result["entry_file"]
        data_dir = result["data_dir"]
        checkpoint_dir = result["checkpoint_dir"]
        hyper_parameters = result["hyper_parameters"]
        req = start_training_req_body(task_id, select_mode, master, peer_nodes_list, server_specification,
                                      server_count, training_engine, code_dir, entry_file, data_dir, checkpoint_dir,
                                      hyper_parameters)
        req.write(p)
        p.writeMessageEnd()
        m.flush()
        return pack_head(m)
    except EOFError:
        print "Error: msg body decode failure"
        return
    except  IOError:
        print "Error: IO Error"
        return