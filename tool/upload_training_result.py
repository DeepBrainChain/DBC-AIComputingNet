import ipfsapi
import os
import commands

api = ipfsapi.connect('127.0.0.1', 5001)

res = api.add('/training_result_file')
res_hash = res["Hash"]
print "training_result_file_ipfspath is:  %s"%(res_hash)

cmd = 'curl "http://122.112.243.44:5001/api/v0/get?arg=/ipfs/%s"'%(res_hash)
(status, output) = commands.getstatusoutput(cmd)
if status == 0:
   print("curl training_result_file_ipfspath success")
else:
   print("curl training result to ipfs error")

