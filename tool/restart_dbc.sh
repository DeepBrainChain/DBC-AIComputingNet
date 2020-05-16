
# ! /bin/sh



START_CMD="sudo systemctl start dbc"
START_CMD_DOCKER="sudo systemctl restart docker"
LOG_FILE_DBC="restartdbc.log"
LOG_FILE_DOCKER="restartdocker.log"

while true 

do


   dbc=`systemctl list-units --type=service | grep "dbc Daemon" | awk '{print  $4}'`

   if [ "$dbc" != "running" ]

    then

        echo "start  dbc service...................."

        echo `date +%Y-%m-%d` `date +%H:%M:%S` 'restart dbc'  >>$LOG_FILE_DBC

        ${START_CMD}

    fi

   docker=`systemctl list-units --type=service | grep "Docker Application Container Engine" | awk '{print  $4}'`

    if [ "$docker" != "running" ]

    then

        echo "restart  docker service...................."

        echo `date +%Y-%m-%d` `date +%H:%M:%S` 'restart docker'  >>$LOG_FILE_DOCKER

        ${START_CMD_DOCKER}

    fi

    sleep 100s


done















