# ! /bin/sh


START_CMD="sudo systemctl start dbc-client"
LOG_FILE_DBC="restartdbc.log"

while true 

do


   dbc=`systemctl list-units --type=service | grep "dbc client Daemon" | awk '{print  $4}'`

   if [ "$dbc" != "running" ]

    then

        echo "start dbc-client service...................."

        echo `date +%Y-%m-%d` `date +%H:%M:%S` 'restart dbc client'  >>$LOG_FILE_DBC

        ${START_CMD}

    fi

    sleep 100s


done
