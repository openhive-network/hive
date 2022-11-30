from pathlib import Path

import re
from typing import Dict, List

import pytest

import test_tools as tt

from ..local_tools import verify_json_patterns, verify_text_patterns
from . import block_log

TOTAL_BALANCES = {
    'hives': sum((balance['hives'] for balance in block_log.ACCOUNTS_BALANCES), start=tt.Asset.Test(0)),
    'hbds': sum((balance['hbds'] for balance in block_log.ACCOUNTS_BALANCES), start=tt.Asset.Tbd(0))
}

__PATTERNS_DIRECTORY = Path(__file__).with_name('response_patterns')


@pytest.mark.replayed_node
def test_list_my_accounts_json_format(wallet_with_json_formatter):
    import_private_keys_for_accounts(wallet_with_json_formatter, block_log.CREATED_ACCOUNTS)
    accounts_summary = wallet_with_json_formatter.api.list_my_accounts()
    for name, balances, returned_account in zip(block_log.CREATED_ACCOUNTS, block_log.ACCOUNTS_BALANCES,
                                                accounts_summary['accounts']):
        assert returned_account['name'] == name
        assert returned_account['balance'] == balances['hives'].as_nai()
        assert returned_account['hbd_balance'] == balances['hbds'].as_nai()

    assert accounts_summary['total_hive'] == TOTAL_BALANCES['hives'].as_nai()
    assert accounts_summary['total_hbd'] == TOTAL_BALANCES['hbds'].as_nai()


@pytest.mark.replayed_node
def test_list_my_accounts_text_format(wallet_with_text_formatter):
    import_private_keys_for_accounts(wallet_with_text_formatter, block_log.CREATED_ACCOUNTS)
    accounts_summary = parse_text_response(wallet_with_text_formatter.api.list_my_accounts())

    for name, balances, returned_account in zip(block_log.CREATED_ACCOUNTS, block_log.ACCOUNTS_BALANCES,
                                                accounts_summary['accounts']):
        assert returned_account['name'] == name
        assert returned_account['balance'] == balances['hives']
        assert returned_account['hbd_balance'] == balances['hbds']

    assert accounts_summary['total_hive'] == TOTAL_BALANCES['hives']
    assert accounts_summary['total_hbd'] == TOTAL_BALANCES['hbds']


@pytest.mark.replayed_node
def test_list_my_accounts_json_format_pattern_comparison(wallet_with_json_formatter):
    import_private_keys_for_accounts(wallet_with_json_formatter, block_log.CREATED_ACCOUNTS)
    accounts_summary = wallet_with_json_formatter.api.list_my_accounts()

    verify_json_patterns(__PATTERNS_DIRECTORY, 'list_my_accounts', accounts_summary)


@pytest.mark.replayed_node
def test_list_my_accounts_text_format_pattern_comparison(wallet_with_text_formatter):
    import_private_keys_for_accounts(wallet_with_text_formatter, block_log.CREATED_ACCOUNTS)
    accounts_summary = wallet_with_text_formatter.api.list_my_accounts()

    verify_text_patterns(__PATTERNS_DIRECTORY, 'list_my_accounts', accounts_summary)


def parse_text_response(text):
    def parse_single_line_with_account_balances(line_to_parse: str) -> Dict:
        splitted_values = re.split(r'\s{2,}', line_to_parse.strip())
        return {
            'name': splitted_values[0],
            'balance': splitted_values[1],
            'vesting_shares': splitted_values[2],
            'hbd_balance': splitted_values[3],
        }

    def parse_single_line_with_total_balances(line_to_parse: str) -> Dict:
        splitted_values = re.split(r'\s{2,}', line_to_parse.strip())
        return {
            'total_hive': splitted_values[1],
            'total_vest': splitted_values[2],
            'total_hbd': splitted_values[3],
        }

    lines = text.splitlines()
    return {
        'accounts': [parse_single_line_with_account_balances(line_to_parse) for line_to_parse in lines[0:3]],
        **parse_single_line_with_total_balances(lines[4]),
    }


def import_private_keys_for_accounts(wallet, account_names: List[str]) -> None:
    for name in account_names:
        wallet.api.import_key(tt.PrivateKey(name))
