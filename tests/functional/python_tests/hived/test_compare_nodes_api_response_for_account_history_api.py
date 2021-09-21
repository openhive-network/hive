# Example usage: pytest -s -n 4 test_account_history.py --ref localhost:8091 --test localhost:8095 --hashes /path/to/hashes.csv -v

from typing import Iterable, Callable, List
import test_tools as tt


def compare(ref_node: tt.RemoteNode, test_node: tt.RemoteNode, foo: Callable, *args) -> bool:
  assert foo(ref_node, *args) == foo(test_node, *args)

def test_get_transactions(ref_node: tt.RemoteNode, test_node: tt.RemoteNode, transactions: List[str]):
  for trx in transactions:
    compare(ref_node, test_node, lambda x, transaction: x.api.account_history.get_transaction(
      id=transaction,
      include_reversible=True
    ), trx)

def test_get_account_history(ref_node: tt.RemoteNode, test_node: tt.RemoteNode, accounts: List[str], request_limit: int):
  for account in accounts:
    compare(ref_node, test_node, lambda x, acc: x.api.account_history.get_account_history(
      account=acc,
      start=-1,
      limit=request_limit,
      include_reversible=True
    ), account)

def test_get_ops_in_block(ref_node: tt.RemoteNode, test_node: tt.RemoteNode, block_range: Iterable[int]):
  for bn in block_range:
    compare(ref_node, test_node, lambda x, block_num: x.api.account_history.get_ops_in_block(
      block_num=block_num,
      only_virtual=False,
      include_reversible=True
    ), bn)

def test_enum_virtual_ops(ref_node: tt.RemoteNode, test_node: tt.RemoteNode, block_range: Iterable[int], request_limit: int):
  for bn in block_range:
    compare(ref_node, test_node, lambda x, block_num : x.api.account_history.enum_virtual_ops(
      block_range_begin=block_num,
      block_range_end=block_num+1,
      include_reversible=True,
      group_by_block=False,
      limit=request_limit,
      operation_begin=0
    ), bn)
