
ipfs upload
-
```
jimmy@jimmy-ThinkPad-E470:~/ipfs/caffe2/mnist$ ipfs add -r code 
added QmTMATCsCr4iwrNahGB71m1QsdXFm8buBjnsXZNmWWXMoJ code/mnist_training.py
added Qmesujxt84THHkxTHhkfLEMmV6DaXvfVGvpgvJNYnCuY5g code/run.sh
added QmPVkVS4hgdsWiBD9KYNSyqiCbD6cAifpWTpaE7jACUMLd code/snapshots/README.md
added QmXrpeKxrwjjTr4oyu88jxsg2h8qPsNDYn2Bzobx3QbSUd code/snapshots
added QmPj1oLCk5GHXKwNWVWrA41zA6CbnriQeJouLtDJ5yF5eF code

jimmy@jimmy-ThinkPad-E470:~/ipfs/caffe2/mnist$ ipfs add -r ./data
added QmPn9wah8y8VrZSfX1qw7TR7VmqqVgS8eNnPHVG3JjcDUU data/data/train.csv
added QmWHh9RMTEd5VX6Kw3UpY4VuMRx29aHfmF6UNeuggCEJsV data/data
added QmfK89jkqHjecNHDnTSmNbK1zc86sTqgfDB3CBgkFZYnC7 data
```

ipfs publis
-
```
curl 'http://114.116.19.45:8080/ipfs/QmPj1oLCk5GHXKwNWVWrA41zA6CbnriQeJouLtDJ5yF5eF'
curl 'http://114.116.19.45:8080/ipfs/QmfK89jkqHjecNHDnTSmNbK1zc86sTqgfDB3CBgkFZYnC7'

```
or
```
http://122.112.243.44:5001/api/v0/get?arg=/ipfs/QmPj1oLCk5GHXKwNWVWrA41zA6CbnriQeJouLtDJ5yF5eF
http://122.112.243.44:5001/api/v0/get?arg=/ipfs/QmfK89jkqHjecNHDnTSmNbK1zc86sTqgfDB3CBgkFZYnC7
```

update docker image
-

    # docker pull dbctraining/caffe2-gpu:v0.0.1





