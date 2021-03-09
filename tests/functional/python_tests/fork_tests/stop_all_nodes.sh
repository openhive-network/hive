#!/bin/bash

kill -s SIGINT `pidof ../../hived`
kill -s SIGINT `pidof ./cli_wallet`

