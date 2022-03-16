# Example usage: pytest -s -n 4 test_account_history.py --ref localhost:8090 --test localhost:8095 [ --start 4900000 --stop 4925000 ]

from pytest import fixture
from typing import Callable, Generator
from test_tools import RemoteNode

LIMIT = 1_000

@fixture
def ref_node(request) -> RemoteNode:
	return RemoteNode(request.config.getoption("--ref"))

@fixture
def test_node(request) -> RemoteNode:
	return RemoteNode(request.config.getoption("--test"))

@fixture
def transactions() -> list:
	from input_data.hashes import HASHES
	return HASHES

@fixture
def accounts() -> list:
	from input_data.accounts import ACCOUNTS
	return ACCOUNTS

@fixture
def block_range(request):
	return range(request.config.getoption("--start"), request.config.getoption("--stop"))

def compare(ref_node : RemoteNode, test_node : RemoteNode, foo : Callable):
	assert foo(ref_node) == foo(test_node)

def test_get_transactions(ref_node : RemoteNode, test_node : RemoteNode, transactions : list):
	ref_node.api.account_history.get_transaction(id=transactions[0], include_reversible=True)
	for trx in transactions:
		compare(ref_node, test_node, lambda node : node.api.account_history.get_transaction(
			id=trx,
			include_reversible=True
		))

def test_get_account_history(ref_node : RemoteNode, test_node : RemoteNode, accounts : list):
	for account in accounts:
		compare(ref_node, test_node, lambda x : x.api.account_history.get_account_history(
			account=account,
			start=-1,
			limit=LIMIT,
			include_reversible=True
		))

def test_get_ops_in_block(ref_node : RemoteNode, test_node : RemoteNode, block_range : Generator):
	for bn in block_range:
		compare(ref_node, test_node, lambda x : x.api.account_history.get_ops_in_block(
			block_num=bn,
			only_virtual=False,
			include_reversible=True
		))

def test_enum_virtual_ops(ref_node : RemoteNode, test_node : RemoteNode, block_range : Generator):
	for bn in block_range:
		compare(ref_node, test_node, lambda x : x.api.account_history.enum_virtual_ops(
			block_range_begin=bn,
			block_range_end=bn+1,
			include_reversible=True,
			group_by_block=False,
			limit=LIMIT,
			operation_begin=0
		))
