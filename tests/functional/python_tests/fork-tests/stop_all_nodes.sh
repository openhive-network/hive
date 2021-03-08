#!/bin/bash

kill -s SIGINT `pidof ../hived`
kill -s SIGINT `pidof ./nodes-eu/cli_wallet`

