#/bin/sh

containers=`docker ps -a | awk '{print $1}' | grep -v CONTAINER`
for line in $containers
do
     echo 'rm container:'
     docker container rm $line
done

