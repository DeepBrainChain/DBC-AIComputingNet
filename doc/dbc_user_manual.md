


This document depicts the basic operations of dbc in three scenario:
* [computing node](#computing-node)
* [client node](#client-node)
* [seed node](#seed-node)


# computing node 
## start 
```
# sudo systemctl start dbc
```
## stop
```
# sudo systemctl stop dbc
```

## configure dbc offline auth node
add trust node, enable running management instructions from these nodes. 
for example, edit core.conf under conf dir.
```
auth_mode=online
trust_node_id=2gfpp3MAB475q4WNyyuL6kPUjw3nedJLKRgiiJiUj7Z
trust_node_id=2gfpp3MAB4F5y2zVd1U7EJjLhMnqZPAcQ9kfpe4jpXZ
```


# client node
## start 
```
# ./dbc
dbc>>>
```
## stop
```
dbc>> exit
```

## configure dbc restful interface
goto dbc install folder, and edit core.conf under conf dir.
```
rest_ip=127.0.0.1
rest_port=41107
```



# seed node