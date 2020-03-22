#!/bin/bash
 cd /home/dbc/ngrok
 if [ ! -f "logs/server.log" ]; then
          bash ./stopapp.sh
          bash ./startapp.sh
 fi