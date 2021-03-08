#!/bin/bash

cd nodes-us
cd node0
screen -md -S nodes-us-node0 ./start_hived.sh

cd ../node1
screen -md -S nodes-us-node1 ./start_hived.sh

cd ../node2
screen -md -S nodes-us-node2 ./start_hived.sh

cd ../../
cd nodes-eu
cd node0
screen -md -S nodes-eu-node0  ./start_hived.sh

cd ../node1
screen -md -S nodes-eu-node1  ./start_hived.sh

cd ../node2
screen -md -S nodes-eu-node2  ./start_hived.sh

cd ../node3
screen -md -S nodes-eu-node3  ./start_hived.sh

