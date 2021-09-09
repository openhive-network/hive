#!/usr/bin/python3

from time import sleep
from test_tools import World, constants, logger
from test_tools.account import Account
from test_tools import Asset
from test_tools.private.node import Node
from datetime import datetime, timedelta
import json


from test_tools.paths_to_executables import set_path_of
# set_path_of('hived', '/home/raidg/hive/build_no_prof/programs/hived/hived')
# node.set_executable_file_path('/home/raidg/hive/build_no_prof/programs/hived/hived')

def updated_conf( world : World ):
	world.set_clean_up_policy(constants.WorldCleanUpPolicy.DO_NOT_REMOVE_FILES)
	return


	# from pytest import raises

	'''
	struct debug_generate_blocks_args 
	{
	std::string                               debug_key;
	uint32_t                                  count = 0;
	uint32_t                                  skip = hive::chain::database::skip_nothing;
	uint32_t                                  miss_blocks = 0;
	bool                                      edit_if_needed = true;
	};

	struct debug_generate_blocks_return
	{
	uint32_t                                  blocks = 0;
	};

	'''
	# gdp_after = node.api.condenser.get_dynamic_global_properties()
def get_datetime_format():
	return '%Y-%m-%dT%H:%M:%S'

def format_datetime(date : datetime)-> str:
	return datetime.strftime(date, get_datetime_format())

def parse_datetime(date : str)-> datetime:
	return datetime.strptime(date, get_datetime_format())

def log_stats(node : Node):
	tm = node.api.condenser.get_dynamic_global_properties()['result']['time']
	logger.info(f"current on-chain time: {tm}")
	block = node.get_last_block_number()
	logger.info(f"current block: {block}")
	return (parse_datetime(tm), block) 

def fast_forward(node : Node, blocks : int, account : Account):
	tm, _ = log_stats(node)
	new_tm = tm + timedelta(seconds=(blocks * 3))
	logger.info(f"forwarding with account: {account.name}")
	logger.info(f"forwarding {blocks} blocks...")
	node.api.debug_node.debug_generate_blocks_until( invoker=account.name, invoker_private_key=account.private_key, fast_forwarding_end_date=format_datetime(new_tm), blocks_per_witness=1 )

def measure_bps(node):
	_, bn_before = log_stats(node)
	logger.info("1 seconds...")
	sleep(1)
	_, bn_after = log_stats(node)
	logger.info(f"currently: {(bn_after-bn_before) / 1.0} bps")

def test_forwarding_blocks(world : World):
	updated_conf(world)

	PARTICIPATION = 0
	print(flush=True)
	net = world.create_network()
	node = net.create_init_node()
	# node.set_executable_file_path('/home/raidg/hive/build_no_prof/programs/hived/hived')
	node.set_requests_logging_policy( constants.LoggingOutgoingRequestPolicy.LOGGING_AS_CURL_REQUEST )
	node.config.enable_stale_production = True
	node.config.required_participation = PARTICIPATION
	# node.set_executable_file_path()

	node.config.plugin.append('debug_node')
	# node.config.plugin.append('wallet_bridge_api')
	node.config.webserver_http_endpoint = '0.0.0.0:43245'
	# debug_key = node.config.private_key[0]
	# node.run( wait_for_live=True )

	witnesses = []
	# node_2 = net.create_api_node()

	if not False:
		node_2, witnesses_2 = net.create_witness_node_and_return_accounts(witnesses=['wit-1', 'wit-2', 'wit-3', 'wit-4'])
		node_2.config.required_participation = PARTICIPATION
		node_2.config.plugin.append('debug_node')
		# node_2.config.webserver_http_endpoint = '0.0.0.0:53245'
		witnesses.extend(witnesses_2)

	if not False:
		node_3, witnesses_3 = net.create_witness_node_and_return_accounts(witnesses=['wit-5', 'wit-6', 'wit-7', 'wit-8'])
		node_3.config.required_participation = PARTICIPATION
		node_3.config.plugin.append('debug_node')
		witnesses.extend(witnesses_3)

	if not False:
		api_node = net.create_api_node()
		api_node.config.required_participation = PARTICIPATION
		api_node.config.plugin.append('debug_node')
		api_node.set_clean_up_policy(constants.WorldCleanUpPolicy.DO_NOT_REMOVE_FILES)

	# starting network
	net.run()
	node.wait_number_of_blocks(5)
	voter = 'initminer'
	wallet = node.attach_wallet()

	initminer = Account('initminer', with_keys=False)
	initminer.private_key = node.config.private_key[0]
	# fast_forward(node, 500, initminer)
	# for i in range(1):
		# measure_bps(node)
		# input(f"press {1-i} more times enter to continue...")
	# witness : Account = witness[0]

	for w in witnesses:
		# measure_bps(node)
		# input(f'enter to continue...')

		wallet.api.import_key(w.private_key)
		wallet.api.create_account_with_keys(
			creator='initminer',
			newname=w.name,
			json_meta='{}',
			owner=w.public_key,
			active=w.public_key,
			posting=w.public_key,
			memo=w.public_key
		)

		with wallet.in_single_transaction():
			wallet.api.transfer('initminer', w.name, Asset.Test(100.0), 'XXXXXXXXXXX')
			wallet.api.transfer('initminer', w.name, Asset.Tbd(100.0), 'XXXXXXXXXXX')
			wallet.api.transfer_to_vesting('initminer', w.name, Asset.Test(100.0))

		wallet.api.update_witness(w.name, f'http://{voter}.hive', w.public_key, { 'account_creation_fee':Asset.Test(1), 'maximum_block_size' : 131072, 'hbd_interest_rate' : 1000 })

		with wallet.in_single_transaction():
			wallet.api.vote_for_witness(w.name, voter, True)
			wallet.api.vote_for_witness(voter, w.name, True)

		voter = w.name

	wit_orginal = witnesses
	witnesses = [ w.name for w in witnesses ]
	witnesses.append('initminer')

	if False and len(witnesses):
		while True:
			current_schedule = node.api.condenser.get_witness_schedule()['result']['current_shuffled_witnesses']
			missing = False
			for w in witnesses:
				if w not in current_schedule:
					missing = True
					break
			if not missing:
				break
			else:
				node.wait_number_of_blocks(1)

	try:
		while(True):
			measure_bps(node)
			val = input(f'^C to end, enter to log stats...')
			print(val)
			if val.isnumeric():
				try:
					# fast_forward(node, int(val))
					fast_forward(node_2, int(val), wit_orginal[0])
					# fast_forward(node_3, int(val))
				except Exception as e:
					print(e)
	except:
		pass

if __name__ == '__main__':
	import pytest, os
	logger.info(' About to start the tests ')
	pytest.main(args=["-s", os.path.abspath(__file__)])
	logger.info(' Done executing the tests ')