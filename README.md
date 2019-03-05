# deepbrainchain
Artificial Intelligence Computing Platform Driven By BlockChain

# build dbc in linux os with dbc compile container
Suppose download dbc source code locates into ~/deepbrainchain folder.
```
    $ cd ~
    $ git clone https://github.com/DeepBrainChain/deepbrainchain.git
    
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
    $ cd ~/deepbrainchain/make
    $ ./clean.sh; ./build.sh
    $ ls ~/deepbrainchain/output/dbc 
```