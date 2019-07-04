

This document depicts the basic design concept of DBC RESTFul API
* dbc uses libevent to implemnt the http server
* dbc uses rapidjason to do json encode/decode  


<div style="text-align:center", align=center>
<img src="./images/dbc_restful_part1.png" alt="samples" width="900" height="480" /><br/>


....

<div style="text-align:center", align=center>
<img src="./images/dbc_restful_part2.png" alt="samples" width="900" height="200" /><br/>


For example

```
# curl -X POST -H "Content-Type: application/json" http://127.0.0.1:41107/api/v1/tasks/start -d 
 '{ "training_engine": "dbctraining/tensorflow1.9.0-cuda9-gpu-py3:v1.0.0", 
 "code_dir": "QmW7xeDshhbJFMiQ1WgtnimUJTd3j3JdfyQdtbdoz9mS6n", 
 "peer_nodes_list": ["2gfpp3MAB4ARp8JAkTBjPyEUdPM2wckxuDgHNQ3AxKA"], 
 "entry_file":"run.sh",
 "server_specification": {"env":{"NVIDIA_VISIBLE_DEVICES":"0"},"ip": "192.168.1.113","port": {"22":"1022"}}}'
```