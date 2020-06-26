#!/bin/bash
#params:
# - ref hived location
# - tested hived location
# - ref blockchain folder location
# - tested blockchain folder location
# - path to directory, where non-empty logs should be generated
# - stop replay at block
# - number of jobs (optional)
# - --dont-copy-config (optional), if passed config.init files are not copied from test directories
#
# WARNING: use absolute paths instead of relative!
#
# sudo ./docker_build_and_run.sh ~/steemit/hive/build/Release/programs/hived ~/steemit/hive/build/Release/programs/hived ~/steemit/hived_data/hivenet ~/steemit/hived_data/hivenet ~/steemit/logs 5000000 12

if [ $# -lt 6 -o $# -gt 8 ]
then
   echo "Usage: reference_hived_location tested_hived_location ref_blockchain_folder_location tested_blockchain_folder_location"
   echo "       logs_dir stop_replay_at_block [jobs [--dont-copy-config]"
   echo "Example: ~/steemit/ref_hived ~/steemit/hive/build/Release/programs/hived ~/steemit/hivenet ~/steemit/testnet"
   echo "         ~/steemit/logs 5000000 12"
   echo "         if <jobs> not passed, <nproc> will be used."
   exit -1
fi

echo $*

JOBS=0

if [ $# -eq 7 ]
then
   JOBS=$7
fi

docker build -t smoketest ../ -f Dockerfile
[ $? -ne 0 ] && echo docker build FAILED && exit -1

docker system prune -f

if [ -e $5 ]; then
   rm -rf $5/*
else
   mkdir -p $5
fi

docker run -v $1:/reference -v $2:/tested -v $3:/ref_blockchain -v $4:/tested_blockchain -v $5:/logs_dir -v /run:/run \
   -e STOP_REPLAY_AT_BLOCK=$6 -e JOBS=$JOBS -e COPY_CONFIG=$8 -p 8090:8090 -p 8091:8091 smoketest:latest
