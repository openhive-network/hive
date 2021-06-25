#!/usr/bin/python3

'''
Usage: 
	./compare_find_account.py -r https://api.hive.blog -c http://127.0.0.1:8091
or 
	./compare_find_account.py -h 
for additional info
'''

from argparse import ArgumentParser
from sys import argv as incoming_arguments
from sys import stdout as STDOUT
from jsondiff import JsonDiffer, diff as json_diff
from json import dumps as json_dump

ACCOUNTS = []
KEYS = set()

OUTPUT_FILENAME = 'results.out'
with open(OUTPUT_FILENAME, 'w') as file:
	file.write('\n')
parser = ArgumentParser()

parser.add_argument('-r', '--reference', type=str, dest='ref',
										help='http address of reference node (Ex: http://127.0.0.1:8080)', default='https://api.hive.blog')
parser.add_argument('-c', '--compare', type=str, dest='comp',
										help='http address of node to compare with')
# parser.add_argument('-a', '--accounts', nargs='+', type=str)
# parser.add_argument('-k', '--keys', nargs='+', type=str)
parser.add_argument('-o', '--output', type=str, dest='out', choices=['STDOUT', 'FILE'], default='FILE', help=f"if file is choosen (default) output is written to: `{OUTPUT_FILENAME}`")
args = parser.parse_args(list(incoming_arguments[1:]))

def request(address : str, body : str) -> dict:
	from requests import post, get
	body = json_dump(body)
	result = get(address, data=body).json()
	if 'error' in result.keys():
		alias = result['error']
		if 'code' in alias.keys() and 'data' in alias.keys() and 'jussi_request_id' in alias['data'].keys(): 
			# jussi doesn't like get, it requiers post
			return post(address, data=body).json()
	return result

def find_account(account: str, addres: str) -> dict:
	if isinstance(account, str):
		account = [account]
	model = {
		"jsonrpc":"2.0",
		"method":"database_api.find_accounts",
		"params": {
			"accounts": account
		},
		"id":1
	}
	return request(addres, model)

def find_account_and_extract_keys(account : str, address : str) -> dict:
	global KEYS
	res = find_account(account, address)
	if isinstance(res, dict) and 'result' in res.keys():
		for item in res['result']['accounts']:
			for key, value in item.items():
				if key in ['owner', 'active', 'posting']:
					for x in value['key_auths']:
						if isinstance(x, str) and x.startswith('STM'):
							KEYS.add(x)
				elif key == 'memo_key':
					KEYS.add(value)
	return res

def get_key_references(key: str, addres: str) -> dict:
	if isinstance(key, str):
		key = [key]
	model = {
		"jsonrpc": "2.0",
		"method": "account_by_key_api.get_key_references",
		"params": {
			"keys": key
		},
		"id": 1
	}
	return request(addres, model)

def lookup_accounts(account : str, address : str) -> dict:
	model = {
		"jsonrpc":"2.0",
		"method": "condenser_api.lookup_accounts",
		"params": [account ,1000],
		"id":1
	}
	return request(address, model)


def extract_from_response(response : dict) -> dict:
	if 'result' in response:
		return response['result']
	elif 'error' in response:
		return response['error']
	else:
		print(response)
		assert False, 'unknown response type'

def compare(fun, *argv):
	reference = extract_from_response( fun( *argv, args.ref ) )
	tested = extract_from_response( fun( *argv, args.comp ) )
	result = json_diff(reference, tested)
	OUTPUT = STDOUT
	if args.out == 'FILE':
		OUTPUT = open(OUTPUT_FILENAME, 'a')
	
	OUTPUT.writelines([
		f"args: {argv}\n",
		f"reference:\n {reference}\n\n",
		f"tested:\n {tested}\n\n",
		f"diff:\n {result}\n\n",
	])

	if OUTPUT != STDOUT: OUTPUT.close()
	return len(result) == 0

def test_collection_varaidic(fn, coll):
	if coll is None or len(coll) == 0:
		return
	for i in range(1,len(coll) + 1):
		for j in range(0, len(coll), i):
			compare(fn, list(coll[j:j+i]))

def test_collection(fn, coll):
	coll_len = len(coll)
	i = 0
	for item in coll:
		assert compare(fn, item), f'missmatch on: {item}'
		print(f' {i} / {coll_len}\r', end='')
		i += 1

def get_all_account_names() -> list:
	def extract(data : dict):
		return data['result']
	reference = args.ref
	total = []
	result = extract(lookup_accounts('', reference))
	total = result
	while len(result) == 1000:
		result = extract(lookup_accounts(total[-1], reference))
		idx = result.index(total[-1]) # in most cases: 0
		total.extend( result[idx + 1:] )

	return total

ACCOUNTS = get_all_account_names()
test_collection(find_account_and_extract_keys, ACCOUNTS)
test_collection(get_key_references, KEYS)


# print(f"comparing results on endpoint `database_api.find_accounts`")
# test_collection(find_account, args.accounts)
# test_collection(get_key_references, args.keys)
