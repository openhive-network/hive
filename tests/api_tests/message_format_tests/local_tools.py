import pytest
from typing import Literal

import test_tools as tt

from ...local_tools import date_from_now


def create_proposal(wallet, account_name):
    wallet.api.post_comment(account_name, 'test-permlink', '', 'test-parent-permlink', 'test-title', 'test-body', '{}')
    # create proposal with permlink pointing to comment
    wallet.api.create_proposal(account_name, account_name, date_from_now(weeks=2), date_from_now(weeks=50),
                               tt.Asset.Tbd(5), 'test subject', 'test-permlink')


def run_for(*node_names: Literal['testnet', 'mainnet_5m', 'mainnet_64m']):
    """
    Runs decorated test for each node specified as parameter.

    Each test case is marked with `pytest.mark.<node_name>`, which allow running only selected tests with
    `pytest -m <node_name>`.

    Allows to perform optional, additional preparations. See `should_prepare` fixture for details.
    """
    return pytest.mark.parametrize(
        'prepared_node',
        [pytest.param((name,), marks=getattr(pytest.mark, name)) for name in node_names],
        indirect=['prepared_node'],
    )
