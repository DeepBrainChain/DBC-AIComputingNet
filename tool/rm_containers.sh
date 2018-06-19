#/bin/sh
echo "Waring: IF you sure to delete all container please input:yes"
read -t 30 -p ">>" param
if [ $param != "yes" ];then
   exit 1
fi

containers=`docker ps -a | awk '{print $1}' | grep -v CONTAINER`
for line in $containers
do
     echo 'rm container:'
     docker container rm $line
done

