#!/bin/bash

screen -md -S eu-cli_wallet ./cli_wallet --chain-id=04e8b5fc4bb4ab3c0ee3584199a2e584bfb2f141222b3a0d1c74e8a75ec8ff39 -s ws://0.0.0.0:3902 -d -H 0.0.0.0:3904 --rpc-http-allowip 192.168.10.10 --rpc-http-allowip=127.0.0.1

screen -md -S us-cli_wallet ./cli_wallet --chain-id=04e8b5fc4bb4ab3c0ee3584199a2e584bfb2f141222b3a0d1c74e8a75ec8ff39 -s ws://0.0.0.0:4902 -d -H 0.0.0.0:4904 --rpc-http-allowip 192.168.10.10 --rpc-http-allowip=127.0.0.1

