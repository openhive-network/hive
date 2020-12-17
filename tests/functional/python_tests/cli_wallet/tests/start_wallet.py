#!/usr/bin/python3

from utils.test_utils import *
from utils.cmd_args   import args
from utils.cli_wallet import CliWallet
from utils.logger     import log, init_logger
from time import sleep
from time import perf_counter as perf
from json import dump

if __name__ == "__main__":
	with CliWallet( args ) as wallet:
		print(f"args:\n\n{wallet.cli_args.args_to_list()}\n\n")
		input()
