#!/bin/bash

cd nodes_us
cd node0
screen -md -S nodes_us_node0 ../../start_hived.sh

cd ../node1
screen -md -S nodes_us_node1 ../../start_hived.sh

cd ../node2
screen -md -S nodes_us_node2 ../../start_hived.sh

cd ../../
cd nodes_eu
cd node0
screen -md -S nodes_eu_node0  ../../start_hived.sh

cd ../node1
screen -md -S nodes_eu_node1  ../../start_hived.sh

cd ../node2
screen -md -S nodes_eu_node2  ../../start_hived.sh

cd ../node3
screen -md -S nodes_eu_node3  ../../start_hived.sh

