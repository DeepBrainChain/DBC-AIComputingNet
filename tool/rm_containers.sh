#/bin/sh
echo "Waring: IF you sure to delete all container please input:yes"
read -t 30 -p ">>" param
if [ $param != "yes" ];then
   exit 1
fi

if [ $# -lt 1 ]; then
    echo "params error"
    echo "input days or all"
    exit
fi
days=$1
if [ $days=="all" ]; then
    echo "rm all"
    docker rm -f $(docker ps -a -q)
    exit
fi

containers=`docker ps -a --format "table {{.ID}}\t{{.CreatedAt}}"  | grep -v CONTAIN | awk '{print $1","$2","$3}'`
cur_time=`date +'%Y-%m-%d %H:%M:%S'`
echo "now:"$cur_time
cur_second=$(date --date="$cur_time" +%s); 
for info in $containers
do
    containerid=`echo $info | cut -d \, -f 1`
    create_date=`echo $info | cut -d \, -f 2` 
    cretae_time=`echo $info | cut -d \, -f 3`
    create_stamp=$create_date" "$cretae_time
    
    create_seconds=$(date --date="$create_stamp" +%s); 
    interval=$((cur_second-create_seconds))
    
    interval=`expr $interval / 86400`
    if [ $interval -ge $days ];then
        echo "rm:"$info
        docker container rm -f $containerid
    fi
   
done

