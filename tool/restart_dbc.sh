
# ! /bin/sh



START_CMD="sudo systemctl start dbc"

LOG_FILE="restart.log"


while true 

do


   dbc=`systemctl list-units --type=service | grep "running dbc Daemon" |wc -l`

    if [ $dbc -eq 0 ]

    then

        echo "start  dbc service...................."

        echo `date +%Y-%m-%d` `date +%H:%M:%S` 'restart dbc'  >>$LOG_FILE

        ${START_CMD}

    fi


    sleep 100s


done















