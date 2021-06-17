#!/usr/bin/python3

import json
import os.path
from concurrent.futures import ThreadPoolExecutor

import test_tools as tt
from test_tools.account import Account
from test_tools import logger as log
import threading

ACCS_FILE = 'acocounts.json'
MAX_RECURRENT_TRASFERS_PER_USER = 255
# USERS_COUNT = 100
USERS_COUNT = 120000
# USERS_COUNT = 1500
FULL_RECOVER_IN_BLOCKS = 2480

dest_accounts = []
accounts = []

class AtomicInteger():
	def __init__(self, value=0):
		self._value = int(value)
		self._lock = threading.Lock()

	def inc(self, d=1):
		with self._lock:
			self._value += int(d)
			return self._value

	def dec(self, d=1):
		return self.inc(-d)    

	@property
	def value(self):
		with self._lock:
			return self._value

	@value.setter
	def value(self, v):
		with self._lock:
			self._value = int(v)
			return self._value


def acc_from_dict(data : dict) -> Account:
	# {"name": "actor1245", "secret": "secret", "private_key": "5KMvRJGKCg6BUNSDP6t44G4BmmAXNw4UPPv2HqHix3r13knWd19", "public_key": "TST6pd4CRiUt86fNwdU6zS8CAvZmuvutQCWzpvCCXYW1GbuDVNnhC"}
	res = Account(data['account_name'], generate_keys=False)
	# res.secret = data["secret"]
	res.private_key = data["private_key"]
	res.public_key = data["public_key"]
	return res

if os.path.exists(ACCS_FILE):
	with open(ACCS_FILE, 'r') as file:
		tmp = json.load(file)
		dest_accounts = [ acc_from_dict(x) for x in  tmp['dest'] ]
		accounts = [ acc_from_dict(x) for x in  tmp['accs'] ]

if len(accounts) < USERS_COUNT:
	dest_accounts = [Account(f"actor{i}") for i in range(MAX_RECURRENT_TRASFERS_PER_USER)]
	accounts = [Account(f"actor{i}") for i in range(MAX_RECURRENT_TRASFERS_PER_USER, MAX_RECURRENT_TRASFERS_PER_USER + USERS_COUNT)]
	with open(ACCS_FILE, 'w') as file:
		json.dump( { "dest": list([x.__dict__ for x in dest_accounts]), "accs": list([x.__dict__ for x in accounts]) }, file )

if len(accounts) > USERS_COUNT:
	accounts = list(accounts[:USERS_COUNT])

print([x.name for x in dest_accounts])
# print(accounts[:100])

with tt.World() as world:
	init = world.create_init_node()
	# --statsd-endpoint	
	# init.produced_files = True # work on previous data
	init.config.plugin.append('network_broadcast_api')
	init.config.plugin.append('debug_node_api')
	init.config.plugin.append('transaction_status')
	init.config.plugin.append('statsd')
	init.config.statsd_endpoint = "127.0.0.1:11012"
	init.config.shared_file_size = "1G"
	init.config.webserver_http_endpoint = "0.0.0.0:59921"
	'''
	init.config.plugin.append('state_snapshot')
	init.config.load_snapshot("snap")
	init.config.snapshot_root_dir("../../snapshot")
	'''
	init.run()
	wallet = init.attach_wallet()

	if True:
		with wallet.in_single_transaction():
			for acc in dest_accounts:
				wallet.api.create_funded_account_with_keys( 'initminer', acc.name, '0.001 TESTS', acc.name, '{}', acc.public_key, acc.public_key, acc.public_key, acc.public_key )
				# wallet.api.transfer('initminer', acc.name, '1.000 TESTS', 'memo')
		init.wait_number_of_blocks(5)
		log.info(f"created destination accounts")

	# https://steemit.com/polish/@abc.love.steemit/how-upvoting-works-on-steemit-vp-voting-power-explained-jak-dziala-glosowanie-upvote-vp-moc-glosowania

		accounts_per_block = 250

		def fast_create(i):
			log.info(f"creating: {i} - {i+accounts_per_block} accounts")
			mwallet = init.attach_wallet()
			with mwallet.in_single_transaction():
				for acc in list(accounts[i:min(i+accounts_per_block,len(accounts))]):
					mwallet.api.create_funded_account_with_keys( 'initminer', acc.name, '0.001 TESTS', acc.name, '{}', acc.public_key, acc.public_key, acc.public_key, acc.public_key )
					mwallet.api.transfer('initminer', acc.name, f'{MAX_RECURRENT_TRASFERS_PER_USER * 0.001 :.3f} TESTS'.replace(',', '.'), 'memo')
			mwallet.close()

		with ThreadPoolExecutor(max_workers=10) as executor:
			for x in [executor.submit(fast_create, i) for i in range(0, min(len(accounts), USERS_COUNT), accounts_per_block)]:
				x.exception()

	print(wallet.api.get_account('initminer'))
	wallet.api.transfer_to_vesting('initminer', 'initminer', '240000000.000 TESTS')
	print(wallet.api.get_account('initminer'))

	# exit(0)
	# print(init.api.database.list_accounts(start=None, limit=100))
	# exit(0)
	# print(wallet.api.get_account(accounts[0].name))
	vests_per_block = 1500
	for i in range(0, min(len(accounts), USERS_COUNT), vests_per_block):
		log.info(f"vesting: {i} - {i+vests_per_block} accounts")
		with wallet.in_single_transaction():
			for acc in list(accounts[i:min(i+vests_per_block,len(accounts))]):
				wallet.api.delegate_vesting_shares('initminer', acc.name, f'{0.000001 + (MAX_RECURRENT_TRASFERS_PER_USER / 5) :.6f} VESTS'.replace(',', '.'))

	# exit(0)

	init.api.debug_node.debug_generate_blocks(debug_key=init.config.private_key[0], count=FULL_RECOVER_IN_BLOCKS)

	log.info(f"created source accounts")
	# init.api.debug_node.debug_generate_blocks(debug_key=init.config.private_key[0], count=120)

	print(wallet.api.get_account(accounts[20].name))
	print(wallet.api.get_account(dest_accounts[0].name))
	# for acc in accounts:
		# wallet.api.import_key(acc.private_key)


	multipler = 4 * 5
	workers_count = 10
	cnter = AtomicInteger(0)
	cv = threading.Condition()

	def create_recurrent_transfers( acc_range : list, id : int ):
		alen = len(acc_range)
		print(f"in: {id}, len: {alen}, name_0: {acc_range[0].name}, name_last: {acc_range[-1].name}")
		my_wallet = init.attach_wallet()
		account_count = 4
		for i in range(0, alen, account_count * multipler):
			for j in range(i, min(i+(account_count * multipler), alen)):
				acc = acc_range[j]
				my_wallet.api.import_key(acc.private_key)
				# print(wallet.api.get_account(acc.name))
				with my_wallet.in_single_transaction():
					for i in range(int(MAX_RECURRENT_TRASFERS_PER_USER / multipler)):
						my_wallet.api.recurrent_transfer(acc.name, dest_accounts[i].name, '0.001 TESTS', str(f"{i} {j}"), 24, 5)
				print(f'done: {acc.name}')

			print(f"done: {acc.name} / {acc_range[-1].name}")

	
	accounts_per_thread = int((len(accounts) / workers_count) + 1)
	for k in range(multipler):
		# with ThreadPoolExecutor(max_workers=10) as executor:
		threads = []
		for i in range(0, len(accounts), accounts_per_thread):
			th = threading.Thread( target=create_recurrent_transfers, args=( list(accounts[i:i+accounts_per_thread]), i ) )
			th.start()
			threads.append( th )
		
		for fut in threads:
			fut.join()

		init.api.debug_node.debug_generate_blocks(debug_key=init.config.private_key[0], count=FULL_RECOVER_IN_BLOCKS)
		print(f"{i} / {multipler}")

	print(wallet.api.get_account(accounts[20].name))

