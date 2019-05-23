from matrix.ttypes import *
from matrix.ttypes_header import *
from matrix.dbc_service import *
from thrift.protocol.TBinaryProtocol import TBinaryProtocol
from thrift.transport.TTransport import TMemoryBuffer
from socket import *
import time
from dbc_message.msg_common import *
import binascii

def make_start_training_req(code_hash, entry_file,engine,peer_nodes,hyper_params,data_hash=""):
    try:
        m = TMemoryBuffer()
        p = TBinaryProtocol(m)
        msg_name = AI_TRAINING_NOTIFICATION_REQ
        nonce = get_random_id()
        head = msg_header(get_magic(), msg_name, nonce)
        # head.write(p)
        task_id = get_random_id()
        print ("task_id: %s, nonce:%s"%(task_id,nonce))
        # select_mode=bytes(result["select_mode"])[0]
        select_mode = 0x00
        master = ""
        pns = peer_nodes
        peer_nodes_list = pns.split(",")

        server_specification =""
        server_count = 0
        #training_engine = result["training_engine"]
        training_engine = engine
        code_dir = code_hash
        entry_file = entry_file
        data_dir = data_hash
        checkpoint_dir = ""
        hyper_parameters = hyper_params
        req = start_training_req_body(task_id, select_mode, master, peer_nodes_list, server_specification,
                                      server_count, training_engine, code_dir, entry_file, data_dir, checkpoint_dir,
                                      hyper_parameters)

        message=task_id+code_dir+nonce
        print("message:",message)
        sign_algo="ecdsa"
        origin=get_node_id()
        # print("sign_origin:", origin)
        exten_info={}
        exten_info["origin_id"]=origin
        exten_info["sign_algo"]=sign_algo
        exten_info["sign"] = dbc_sign(message)
        print("sign:", exten_info["sign"])
        head.exten_info = exten_info
        head.write(p)
        req.write(p)
        p.writeMessageEnd()
        m.flush()
        return task_id, pack_head(m)
    except EOFError:
        print("Error: msg body decode failure")
        return
    except  IOError:
        print ("Error: IO Error")
        return