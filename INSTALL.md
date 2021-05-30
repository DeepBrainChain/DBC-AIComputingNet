客户端节点
=======================================================
安装：
```
    tar -zxvf dbc-linux-client-0.3.7.3.tar.gz
    cd ipfs_repo
    /bin/bash install_ipfs.sh go-ipfs_v0.4.15_linux-amd64.tar.gz
    cd dbc_repo
    修改conf目录下的core.conf配置文件：
        net_type=mainnet
        magic_num=0xF1E1B0F9
        rest_ip=0.0.0.0
        main_net_listen_port=11118
```
启动：
```
    ./dbc -d
```
