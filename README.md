# deepbrainchain
Artificial Intelligence Computing Platform Driven By BlockChain

If you want to add your machine to the DBC network, go straight to the “install DBC computing node” and ignore the rest
如果你想要添加机器到DBC网络中，直接查看“如何安装DBC计算节点”，其他可以忽略

# build dbc in linux os with dbc compile container
Suppose download dbc source code locates into ~/deepbrainchain folder.
```
    $ cd ~
    $ git clone https://github.com/DeepBrainChain/DBC-AIComputingNet.git
    $ git checkout dev
    
    $ docker pull dbctraining/dbc_compile:v3
    
    $ docker run -it --rm -v ~/deepbrainchain:/home/deepbrainchain dbctraining/dbc_compile:v3 /bin/bash
    
    # cd /home/deepbrainchain/make
    # ./clean.sh; ./build.sh
    
    # cd /home/deepbrainchain/deployment
    # bash ./package.sh
    # ls ./package
```



# build dbc in mac osx
   
prerequisite

* xcode with command line tool support
* boost 1.6x, for example, brew install boost    

   
```
    $ cd ~
    $ git clone https://github.com/DeepBrainChain/DBC-AIComputingNet.git
    $ git checkout dev
    
    $ cd ~/deepbrainchain/make
    $ ./clean.sh; ./build.sh
    $ ls ~/deepbrainchain/output/dbc 
```


DBC计算节点部署
一、安装linux操作系统
操作系统版本要求： os ubuntu 16.04 LTS或者os ubuntu 18.04 LTS或者os ubuntu 19.04 LTS
操作系统镜像地址（以下地址都可以下载）：
http://116.85.24.172:20444/static/ubuntuOS/ubuntu-16.04-desktop-amd64.iso 
或者
http://old-releases.ubuntu.com/releases/19.04/ubuntu-16.04-desktop-amd64.iso

http://116.85.24.172:20444/static/ubuntuOS/ubuntu-18.04-desktop-amd64.iso 
或者
http://old-releases.ubuntu.com/releases/19.04/ubuntu-18.04-desktop-amd64.iso

http://116.85.24.172:20444/static/ubuntuOS/ubuntu-19.04-desktop-amd64.iso 
或者
http://old-releases.ubuntu.com/releases/19.04/ubuntu-19.04-desktop-amd64.iso
二、安装前准备，解决网络连通问题以及docker安装问题
#  echo "nameserver 8.8.4.4" | sudo tee /etc/resolv.conf > /dev/null
#  echo "nameserver 8.8.4.4" >> /etc/resolvconf/resolv.conf.d/base
#  echo "nameserver 114.114.114.114" >> /etc/resolvconf/resolv.conf.d/base
#  echo "nameserver 8.8.8.8" >> /etc/resolvconf/resolv.conf.d/base
#  apt update;apt upgrade
#  apt-get  install -f gcc make  build-essential pkg-config linux-headers-`uname -r` 

三、固定系统内核版本
执行如下命令，固定系统内核版本，防止内核自动升级后nvidia驱动丢失
# VERSION=$(uname -r)
# sudo sed -i "s/GRUB_DEFAULT=0/GRUB_DEFAULT=\"Advanced options for Ubuntu>Ubuntu, with Linux ${VERSION}\"/" /etc/default/grub ;sudo update-grub
四、安装显卡驱动
# vi /etc/modprobe.d/blacklist.conf 
在文件末尾添加如下几行：
blacklist rivafb
blacklist vga16fb
blacklist nouveau
blacklist nvidiafb
blacklist rivatv
options nouveau modeset=0
# sudo update-initramfs -u -k all ：更新内核
# reboot：重启机器
# sudo wget http://116.85.24.172:20444/static/nvidia_driver/NVIDIA-Linux-x86_64-440.44.run
# sudo chmod +x ./NVIDIA-Linux-x86_64-440.44.run
# sudo ./NVIDIA-Linux-x86_64-440.44.run --no-x-check --no-nouveau-check --no-opengl-files 

五、创建和挂载XFS文件系统（只有此文件系统才能限制docker的硬盘空间大小）
1．查看磁盘的分区情况：df -T -h


2．确认将要被dbc使用的硬盘分区是否是XFS系统（给dbc分配不少于1T的硬盘空间，实际越大越好，越大机器被用户租用的概率越高）。从红框中可以看出文件系统已经是xfs，则步骤五，可以跳过。如果不是，则需要进行如下步骤创建XFS文件系统。

3．安装 XFS系统工具集：sudo apt-get install xfsprogs

4．创建一个分区(如果已经有一个可以用的分区，则跳过此步骤)，假设你的分区在/dev/sda,执行：sudo fdisk /dev/sda 然后设置分区sda1

5．对将要格式化为XFS类型的分区先解除挂载（如果是刚创建新的分区，则跳过此步骤），umount  /dev/xxxx 假设要解除挂载的分区是/dev/sda1,执行：umount /dev/sda1
如果出现umount: /xxx: device is busy.等字样。先杀死占用的进程，执行：fuser -mv /xxxx。假设原来挂载在/data下面，执行：fuser -mv /data  查看有哪些进程，执行：kill -9 进程id，杀死进程。如果只有下图内容，说明所有进程都被杀死。


6．格式化分区为XFS(格式化之前要把当前分区中的数据备份，否则格式化后会丢失)，假设此创建的分区叫/dev/sda1。执行：sudo mkfs.xfs -f /dev/sda1

7．重新挂载分区：假设希望把/dev/sda1挂载到/data，执行：sudo mount  -o pquota /dev/sda1 /data。如果/data还不存在，则先执行：sudo -i，然后执行：cd  /,然后执行：mkdir data，再执行：

8．查看XFS挂载是否成功：df -Th /data
9．查看挂载的磁盘分区UUID：sudo blkid



10．配置开机自动挂载：sudo vi /etc/fstab 如果不进行此操作，开机后，磁盘将不会挂载，DBC将无法正常运行。加入UUID=8967b994-ff96-4070-8537-ce1a58b17d38  /data     xfs pquota 0 1,其中UUID为你查询出来的UUID值

11．修改完/etc/fstab文件后，运行sudo mount -a
验证配置是否正确，没有任何提示，表示配置成功。配置不正确可能会导致系统无法正常启动。



六、确认机器系统的语言为英文，如果不是请修改
七、机器添加swap分区
# sudo dd if=/dev/zero of=/mnt/swap bs=1M count=xxxx （此指令执行速度很慢，要等待比较长的时间）
此处xxxx应该等于机器内存数值一半，可以通过free -m查看机器内存。Total的值为内存值，所以xxxx为：64404=128808/2 .
如果机器内存大于200000，则swap值最多设置100000

# sudo mkswap /mnt/swap
# sudo swapon /mnt/swap
# 设置开机时自启用 SWAP 分区：sudo echo "/mnt/swap swap swap defaults 0 0" >> /etc/fstab
# 查看是否成功：cat /proc/swaps
八、设置docker开启SWAP分区
# vi  /etc/default/grub
添加： GRUB_CMDLINE_LINUX="cgroup_enable=memory  swapaccount=1"
（把原来的GRUB_CMDLINE_LINUX=""删除掉）
# sudo update-grub
# 重启系统：reboot
九、创建dbc用户
# wget http://116.85.24.172:20444/static/add_dbc_user.sh
# chmod +x add_dbc_user.sh
# sudo ./add_dbc_user.sh dbc

十、安装dbc节点程序
注意：需要切换到dbc用户安装
# su - dbc  过程中需要设置dbc用户密码
# mkdir install; cd install
# wget http://116.85.24.172:20444/static/ai_miner_install_0.3.7.3.sh
# bash ./ai_miner_install_0.3.7.3.sh -d
# bash ./ai_miner_install_0.3.7.3.sh -i /home/dbc

安装过程中，会出现选择docker默认路径，选择 /data这个文件夹（务必选择/data,否则以后容易出问题）

输入您的钱包地址（如果机器被租用，dbc将直接转入该钱包地址，请确保地址信息正确）

十一、重新启动dbc服务
# sudo systemctl stop dbc
# sudo systemctl start dbc

十二、检查dbc服务状态
# sudo systemctl status dbc

十三、定期清理机器缓存
# sudo -i
# touch /root/clean_cache.cron 
# echo '0 */4 * * * sh  /home/dbc/0.3.7.3/dbc_repo/tool/clean_cache.sh' >> /root/clean_cache.cron
# crontab /root/clean_cache.cron


十四、拉取dbc AI深度学习镜像
# sudo docker pull www.dbctalk.ai:5000/dbc-ai-training-tf114-pytorch12:v1.1.3
# sudo docker pull www.dbctalk.ai:5000/dbc-ai-training-tf2-pytorch14:v1.0.5
# sudo docker pull www.dbctalk.ai:5000/dbc-ai-training-mds:v1.0.2

这3个镜像下载完毕后，确定一下占用空间大小，是否与红框一致（docker images查看）


如果不一致，执行docker rmi xxxxx，删除后，重新拉取。xxxxxx为镜像id



十五、设置IP信息（如果当前机器有固定独立外网IP地址，则此步骤可以跳过.如果没有固定外网IP地址，务必执行下面步骤，否则如果机器是动态IP，IP发生变化，会导致用户无法使用，DBC会被扣除）
1.如果机器可以通过域名访问：
# vi /home/dbc/0.3.7.3/dbc_repo/.dbc_node_info.conf
# ip修改成 ip=域名，然后保存 (如果没有ip这一项，先执行第17步)
# sudo systemctl restart dbc

2.如果机器没有独立IP，也不能通过域名访问：
# vi /home/dbc/0.3.7.3/dbc_repo/.dbc_node_info.conf
# ip修改成 ip=N/A，保存 (如果没有ip这一项，先执行第17步)
# sudo systemctl restart dbc

提示：建议机器有自己的固定ip地址或者固定域名，这样会提升机器的数据传输速度，用户满意度更高，机器出租的饱和度会增加
十六、查看机器node_id，在网站中添加（需要5-10分钟时间等待网络同步机器信息）
# vi /home/dbc/0.3.7.3/dbc_repo/dat/node.dat
注意：当在网站添加机器，系统会开始验证机器的可用性，验证的时候，不能重启dbc，否则会验证失败
十七、机器ID和私钥备份（重要）
#备份如下文件内容：vi  /home/dbc/0.3.7.3/dbc_repo/dat/node.dat，放到安全的位置，后面如果重装系统或者重装DBC需要用到

十八、个人钱包地址查看
# vi /home/dbc/0.3.7.3/dbc_repo/conf/core.conf
十九、参数检查
#检查内存、硬盘、显卡、IP，如果在网站上没有看到下图的内容，说明系统没有检测到内存或者硬盘，需要手动执行一次检查命令：
bash   /home/dbc/0.3.7.3/dbc_repo/tool/node_info/node_info.sh
# 执行完成后，第15步需要重新再操作一遍
# 重启DBC：sudo systemctl restart dbc
# 等待3-10分钟，网站会自动同步数据，如果10分钟后，还没有同步数据，可以联系：support@dbchain.ai 技术支持


二十、添加成功后，自己租用机器试试，看看能否正常租用
# 租用成功后，会收到邮件，根据邮件里面的操作，登陆机器，如果可以顺利登陆，则机器添加成功。如果不能成功登陆，可以联系：support@dbchain.ai 技术支持
二十一、机器分类说明
上线DBC网络的机器根据机器的硬盘空间和内存空间会自动分为3类机器，分别是：
I类： 可租用CPU容器和GPU容器的AI机器，需满足：每个GPU可分配内存空间大于14G且每个GPU可分配硬盘空间大于160G
II类：只可租用GPU容器的AI机器,需满足：每个GPU可分配内存空间大于10G且每个GPU可分配硬盘空间大于60G

III类：挖矿机器：每个GPU可分配内存空间小于10G或者每个GPU可分配硬盘空间小于60G

当机器被判定为挖矿机器，则机器在人工智能云平台机器列表中无法显示，只能在专门挖矿的云算力平台机器列表中显示
如果想要将机器从III类机器提升为I类机器：有如下3种方法：
#加大机器的内存空间和硬盘空间
#减少GPU数量
#在网站选项中增加保留的GPU数量






二十二、出租说明
#用户租用的机器列表中最长可租用时间是你设置的机器结束时间减去8天。因为最后的8天只允许用户从我的停止容器中启动，不能够新租用机器。
#如果要重新设置租用结束时间，租用结束时间是不能小于当前正在使用的用户租用结束时间+8*24小时
二十三、收入说明
#添加机器后，用户租用机器，可以有dbc收入。
用户支付的dbc最长会在智能合约中质押144小时，超过144小时，每隔6个小时，会自动打币到钱包地址，
如果用户租用时间长度不足144小时，订单结束打币到钱包地址

#点击机器id，可以查看当前机器的订单信息


二十四、用户租用说明
#用户租用分为GPU容器和CPU容器，用户租用GPU容器，机器可以获得DBC收入，用户租用CPU容器机器是没有DBC收入的
#CPU容器分为付费模式和质押模式，用户选择付费模式，DBC收入归平台所得，用户选择质押模式，用户在使用完CPU容器过后，DBC会退还给用户。
#用户租用GPU，硬盘空间付费部分收入归平台所得
二十五、惩罚机制
#如果用户在使用过程中机器出现断网或者断电的情况，质押在智能合约中的代币将会自动退还给用户，最多退还144小时的dbc给用户。
二十六、下线机器
#如果当前机器有用户正在使用，则不能立即下线机器，否则用户无法使用，将会被扣除DBC.
#为了防止新用户使用，可以将机器的起始出租时间设置为xxx小时后，这样新用户就无法租用机器。等到老用户使用完成，就可以进行维护。

二十七、重装系统或者重新安装DBC注意事项（非常重要）
#重装系统之前一定要把 /data文件夹下面的数据备份（如果系统盘和数据盘不是一个物理硬盘可以不用备份，但是在重装系统的时候注意不能把数据删除覆盖了），这里面全部是用户数据，不能丢失
#重新安装DBC，安装之前必须备份如下文件：/home/dbc/0.3.7.3/dbc_repo/dat/node.dat，然后替换安装DBC后新生成的这个文件，最后重新启动DBC,则所有信息都会被保留

二十八、如果docker出现异常，怎么手动安装Docker
#先卸载docker
sudo apt-get -y purge docker-ce
sudo apt-get -y purge docker-ce-cli

#安装docker 
cd  /home/dbc/0.3.7.3/mining_repo/archive

（下面是一个命令，直接全部拷贝运行，不能分开执行）
sudo dpkg -i ./containerd.io_1.2.12-1_amd64.deb  ./docker-ce-cli_19.03.6~3-0~ubuntu-xenial_amd64.deb ./docker-ce_19.03.6~3-0~ubuntu-xenial_amd64.deb

#修改docker默认文件路径
vi /lib/systemd/system/docker.service
把包含ExecStart= 这行内容删除
添加：ExecStart=/usr/bin/dockerd -H fd:// -H tcp://127.0.0.1:31107 -H unix:///var/run/docker.sock --data-root="/data/docker_data" --add-runtime=nvidia=/usr/bin/nvidia-container-runtime  --live-restore
然后保存

#最后重启docker
systemctl daemon-reload
systemctl restart docker

二十九、如果显卡驱动丢失了，怎么处理？
# 安装显卡驱动
# 安装成功后执行：bash  /home/dbc/0.3.7.3/dbc_repo/tool/node_info/node_info.sh
# 执行完成后，第15步需要重新再操作一遍
# 重启DBC：sudo systemctl restart dbc
# 等待3-10分钟，网站会自动同步gpu数量，如果10分钟后，还没有同步gpu数量，可以联系：support@dbchain.ai 技术支持

