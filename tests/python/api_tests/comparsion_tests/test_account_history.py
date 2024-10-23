# Example usage: pytest -s -n 4 test_account_history.py --ref localhost:8090 --test localhost:8095 [ --start 4900000 --stop 4925000 ]
from __future__ import annotations

from functools import partial
from typing import Callable, Generator

import pytest

import test_tools as tt

LIMIT = 1_000


@pytest.fixture()
def ref_node(request) -> tt.RemoteNode:
    return tt.RemoteNode(request.config.getoption("--ref"))


@pytest.fixture()
def test_node(request) -> tt.RemoteNode:
    return tt.RemoteNode(request.config.getoption("--test"))


@pytest.fixture()
def transactions() -> list:
    from .input_data.hashes import HASHES

    return HASHES


@pytest.fixture()
def accounts() -> list:
    from .input_data.accounts import ACCOUNTS

    return ACCOUNTS


@pytest.fixture()
def block_range(request):
    return range(request.config.getoption("--start"), request.config.getoption("--stop"))


def compare(ref_node: tt.RemoteNode, test_node: tt.RemoteNode, foo: Callable):
    ref_out = foo(ref_node)
    test_out = foo(test_node)
    if ref_out != test_out:
        ref_out_json = ref_out.json()
        test_out_json = test_out.json()
        if ref_out_json != test_out_json:
            tt.logger.warning("models and jsons are different")
        else:
            tt.logger.warning("only models are different, jsons are same")

        tt.logger.error(ref_out_json)
        tt.logger.error(test_out_json)
        pytest.fail("ref_out != test_out")


def test_get_transactions(ref_node: tt.RemoteNode, test_node: tt.RemoteNode, transactions: list):
    ref_node.api.account_history.get_transaction(id=transactions[0], include_reversible=True)
    for trx in transactions:
        compare(
            ref_node,
            test_node,
            partial(
                lambda node, _trx: node.api.account_history.get_transaction(id=_trx, include_reversible=True), _trx=trx
            ),
        )


def test_get_account_history(ref_node: tt.RemoteNode, test_node: tt.RemoteNode, accounts: list):
    for account in accounts:
        compare(
            ref_node,
            test_node,
            partial(
                lambda node, _account: node.api.account_history.get_account_history(
                    account=_account, start=-1, limit=LIMIT, include_reversible=True
                ),
                _account=account,
            ),
        )


def test_get_ops_in_block(ref_node: tt.RemoteNode, test_node: tt.RemoteNode, block_range: Generator):
    for bn in block_range:
        compare(
            ref_node,
            test_node,
            partial(
                lambda node, _bn: node.api.account_history.get_ops_in_block(
                    block_num=_bn, only_virtual=False, include_reversible=True
                ),
                _bn=bn,
            ),
        )


def test_enum_virtual_ops(ref_node: tt.RemoteNode, test_node: tt.RemoteNode, block_range: Generator):
    for bn in block_range:
        compare(
            ref_node,
            test_node,
            partial(
                lambda node, _bn: node.api.account_history.enum_virtual_ops(
                    block_range_begin=_bn,
                    block_range_end=_bn + 1,
                    include_reversible=True,
                    group_by_block=False,
                    limit=LIMIT,
                    operation_begin=0,
                ),
                _bn=bn,
            ),
        )
