#!/bin/bash

echo /tmp/core | tee /proc/sys/kernel/core_pattern
ulimit -c unlimited

# if we're not using PaaS mode then start hived traditionally with sv to control it
if [[ ! "$USE_PAAS" ]]; then
  mkdir -p /etc/service/hived
  cp /usr/local/bin/hive-sv-run.sh /etc/service/hived/run
  chmod +x /etc/service/hived/run
  runsv /etc/service/hived
elif [[ "$IS_TESTNET" ]]; then
  /usr/local/bin/pulltestnetscripts.sh
else
  /usr/local/bin/startpaashived.sh
fi
