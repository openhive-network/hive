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
		def print_account( name : str, _print : bool = True ) -> dict:
			usr = wallet.get_account( name )['result']

			def print_value( _val ):
				log.info( f"_val: {usr[_val]}" )

			if _print:
				log.info( f"account name: {name}" )
				print_value( 'balance' )
				print_value( 'savings_balance' )
				print_value( 'hbd_balance' )
				print_value( 'reward_vesting_balance' )
				print_value( 'reward_vesting_hive' )
				print_value( 'vesting_shares' )
				print_value( 'delegated_vesting_shares' )
				print_value( 'received_vesting_shares' )
				print_value( 'vesting_withdraw_rate' )
				print_value( 'post_voting_power' )
			return usr

		print('a')
		creator, user_1 = make_user_for_tests(wallet)
		sleep(1)
		print('a')
		creator, user_2 = make_user_for_tests(wallet)
		sleep(1)

		print('a')
		i_stats_user_1 = print_account( user_1 )
		print('a')
		i_stats_user_2 = print_account( user_2 )
		print('a')
		i_stats_creator = print_account( creator )
		print('a')
		sleep(1)

		print(f'transfering all money from {creator} to {user_1}')
		wallet.transfer( creator, user_1, i_stats_creator['balance'], 'funds to spam', 'true' )
		sleep(1)
		print_account(creator)
		print_account(user_1)

		print(f'changing transaction operation to `3599`')
		wallet.set_transaction_expiration( "3599" )

		print(f"generating 10'000 transfers")
		start = perf()
		gen = wallet.transfers( user_1, user_2, "0.001 TESTS", 'spam_1', '10000' )
		print(f"Generated 10'000 transfers in: { perf() - start :.2f}s")
		with open("operations.json", 'w') as file:
			dump( gen, file )
