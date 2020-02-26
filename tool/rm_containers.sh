
#/bin/bash
interval=$1
scale=$2
docker_root=$(docker info --format '{{.DockerRootDir}}')

function print_usage()
{
    echo "Usage: prune docker container which have been stopped more than a interval. and if the use ratio of docker root shoul smaller than a value."
    echo "Options:"
    echo "  stop_interval  use_ration  # prune the containers that have been stopped more than stop_interval(1~8640) hour, and the disk use ratio should smaller than use_ration%(1~100) "
    echo "  help                       # rm_containers useage"
    echo "example:"
    echo "  rm_containers  100  60     # prune the containers that have been stopped more than 100 hour, or the disk use ratio should smaller than 60%"
}

function rm_container()
{
    cur_second=$(date +%s)
    p_interval=$2
    for info in $1
   do
        time_stamp=`echo $info | cut -d \, -f 1`
        containerid=`echo $info | cut -d \, -f 2`
        t_seconds=$(date --date="$time_stamp" +%s);
        interval=$(((cur_second-t_seconds)/3600))
        if [ $interval -ge $p_interval ];then
            docker container rm -f $containerid
        fi
    done
}

function rm_image()
{
    cur_second=$(date +%s)
    p_interval=$2
    for info in $1
    do
        time_stamp=`echo $info | cut -d \, -f 1`
        imageid=`echo $info | cut -d \, -f 2`
        t_seconds=$(date --date="$time_stamp" +%s);
        interval=$(((cur_second-t_seconds)/3600))
        if [ $interval -ge $p_interval ];then
            docker  rmi  $imageid
        fi
    done
}

function prune_container_template()
{
    p_interval=$1
    st_template=$2
    time_template=$3
    containers=$(docker ps -a --filter "status=$st_template" --format "{{.ID}}")
    if [ -z "$containers" ];then
        return
    fi
    containers_f=`docker inspect --format "{{.$time_template}},{{.ID}}" $containers`
    rm_container "$containers_f" $p_interval
    
    if [ $p_interval -eq 0 ];then
        exit
    fi
    usage=$(df -h -l /var/lib/docker | grep -v Use | awk '{print $5}' | cut -d \% -f 1)
   # if [ $usage -gt $scale ];then
   #     p_interval=$((p_interval/2))
   #    prune_container_template $p_interval $st_template $time_template
   # fi
}

function prune_image_template_dbc-free-container()
{
    p_interval=$1
    st_template=$2

    images=$(docker images | grep "dbc-free-container"  | awk '{print $3}')
    if [ -z "$images" ];then
         return
    fi
    images_f=`docker inspect --format "{{.$st_template}},{{.ID}}" $images`
    rm_image "$images_f" $p_interval

    if [ $p_interval -eq 0 ];then
        exit
    fi


}


function prune_image_template_none()
{
    p_interval=$1
    st_template=$2



    images=$(docker images | grep "<none>"  | awk '{print $3}')
    if [ -z "$images" ];then
         return
    fi


    images_f=`docker inspect --format "{{.$st_template}},{{.ID}}" $images`
    rm_image "$images_f" $p_interval

    if [ $p_interval -eq 0 ];then
        exit
    fi


}



function prune_container()
{
    p_interval=$1 
    echo "rm exited containers"
    prune_container_template $p_interval "exited" "State.FinishedAt"
    #the container was created, but it was never running.
    echo "rm created, but never running containers"
    prune_image_template_dbc-free-container $p_interval "created" "Created"
    prune_image_template_none $p_interval "created" "Created"
     #the image was created
    echo "rm image"
    prune_image_template $p_interval  "Created"
}

if [ $# -eq 0 ];then
    print_usage
    exit
fi
if [ $1 == "help" ]; then   
    print_usage
    exit
fi
if [ ! $# -eq 2 ];then
    echo "invalid param"
    print_usage
    exit
fi

if [[ $1 == *[!0-9]* ]] || [[ $2 == *[!0-9]* ]]; then
    echo "invalid param"
    print_usage
    exit
fi

if [ $1 -lt 1 ] || [ $1 -gt 8640 ] || [ $2 -lt 1 ] || [ $2 -gt 100 ];then
    echo "invalid param"
    print_usage
    exit
fi

prune_container $interval
