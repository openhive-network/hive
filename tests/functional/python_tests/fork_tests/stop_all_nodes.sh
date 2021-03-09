#!/bin/bash

kill -s SIGINT `pidof ../hived`
kill -s SIGINT `pidof ./nodes_eu/cli_wallet`

