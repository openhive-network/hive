#!/usr/bin/python3

from utils.test_utils import *
from utils.cmd_args   import args
from utils.cli_wallet import CliWallet
from utils.logger     import log, init_logger
from time import sleep
from time import perf_counter as perf
from json import dump
from resources.configini import config

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

		users = create_users(wallet, 3)
		creator = wallet.cli_args.creator
		# print(json.dumps(users))

		users_stats = dict([ (user, print_account( user )) for user in users.keys() ])
		users_stats[creator] = print_account( creator )
		[ wallet.update_witness( name, f'www.{name}.org', keys['pub'], '{}', 'true' ) ]

		begin_http_port = int(wallet.cli_args.rpc_http_endpoint.split(':')[-1])
		begin_ws_port = int(wallet.cli_args.rpc_tls_endpoint.split(':')[-1])
		for name, keys in users.items():
			config( private_key=keys['prv'], witness=f'"{name}"', webserver_http_endpoint=f"127.0.0.1:{begin_http_port}", webserver_ws_endpoint=f"127.0.0.1:{begin_ws_port}" ).generate( f"config_{name}.ini" )
			begin_http_port+=1
			begin_ws_port+=1


		creators_balance = Asset( users_stats[creator]['balance'] )
		balance = creators_balance
		balance.amount *= 0.025

		print(f'transfering all money from {creator} to {user_1}')
		for user in users.keys():
			wallet.transfer( creator, user, str(balance), 'funds to spam', 'true' )

		print(f'changing transaction operation to `3599`')
		wallet.set_transaction_expiration( "3599" )

		transfers = balance.amount / balance.satoshi().amount
		print(f"generating {transfers} transfers")
		start = perf()
		gen = wallet.transfers( user_1, user_2, str(balance.satoshi()), 'spam_1', str(transfers) )
		print(f"Generated 10'000 transfers in: { perf() - start :.2f}s")
		with open("operations.json", 'w') as file:
			dump( gen, file )
