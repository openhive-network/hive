#!/bin/bash
sudo iptables-restore < iptables_backup.conf 

DURATION=$1
re='^[0-9]+$'
if ! [[ $1 =~ $re ]] ; then
     echo "number not provided, turning off connecion between eu  and us for 120 seconds"
     DURATION="120"
else
     echo "turning off connection between eu and us for ${DURATION} seconds"
fi

sleep $DURATION

sudo iptables -F INPUT
