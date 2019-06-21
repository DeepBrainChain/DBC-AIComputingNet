# deepbrainchain
Artificial Intelligence Computing Platform Driven By BlockChain

# build dbc in linux os with dbc compile container
Suppose download dbc source code locates into ~/deepbrainchain folder.
```
    $ cd ~
    $ git clone https://github.com/DeepBrainChain/deepbrainchain.git
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
    $ git clone https://github.com/DeepBrainChain/deepbrainchain.git
    $ git checkout dev
    
    $ cd ~/deepbrainchain/make
    $ ./clean.sh; ./build.sh
    $ ls ~/deepbrainchain/output/dbc 
```


# install dbc computing node

## step 1: os install 

### install os ubuntun 16.04 LTS from image
http://nl.releases.ubuntu.com/16.04/ubuntu-16.04.6-desktop-amd64.iso

### lock linux kernel version for next system restart
<1> get the kernel version
```
 # uname -r
 4.15.0-39-generic
```

<2> edit /etc/default/grub, set Linux kernel version， for example 4.15.0-39-generic
```
# sudo vim /etc/default/grub
GRUB_DEFAULT="Advanced options for Ubuntu>Ubuntu, with Linux  4.15.0-39-generic"
 
# sudo update-grub
```

## step 2: install nvidia driver

<1> install driver run file, or install from 3rd party repo
```
# wget http://116.85.24.172:20444/static/NVIDIA-Linux-x86_64-410.66.run

# sudo chmod +x ./NVIDIA-Linux-x86_64-410.66.run
# sudo ./NVIDIA-Linux-x86_64-410.66.run -no-x-check -no-nouveau-check -no-opengl-files
```

or install from repo of 3rd party
``` 
# sudo add-apt-repository ppa:graphics-drivers
# sudo apt-get update
# sudo apt-get install nvidia-410
```

## step 3: create dbc user
```
# wget http://116.85.24.172:20444/static/add_dbc_user.sh
# chmod +x add_dbc_user.sh
# ./add_dbc_user.sh dbc（this requires sudo if there is no root user access）
```
You might need to enter dbc user password in the process of creation.
Open a new terminal and switch to dbc user:  su - dbc


## step 4: install dbc program 
note: login with dbc instead of root
```
# mkdir install; cd install
# wget http://116.85.24.172:20444/static/ai_miner_install.sh
# bash ./ai_miner_install.sh -d
# bash ./ai_miner_install.sh -i /home/dbc
```

## step 5: start dbc service
``` 
# sudo systemctl start dbc
```

# upgrade dbc program in computing node
precondition: dbc service is running
```
# systemctl status dbc
```

login with dbc, then run, e.g. upgrade to dbc 0.3.6.7  
```
# mkdir upgrade; cd upgrade
# wget http://116.85.24.172:20444/static/ai_miner_upgrade.sh
# bash ./ai_miner_upgrade.sh -t 0.3.6.7
```

# install dbc client program
linux, support ubuntu 16.04
```
# cd ~
# wget  http://116.85.24.172:20444/static/dbc_client_linux.tar.gz
# tar xvzf ./dbc_client_linux.tar.gz
# cd ./dbc_client_linux
# ./dbc
```
macos, support osx 10.14. Open a terminal
```
# wget  http://116.85.24.172:20444/static/dbc_client_macos.tar.gz
# tar xvzf ./dbc_client_macos.tar.gz
# cd ./dbc_client_macos
# ./dbc
```

# dbc user manual
refer to [doc/dbc_user_manual.md](doc/dbc_user_manual.md) for more details about start/stop dbc and configuration.
 



