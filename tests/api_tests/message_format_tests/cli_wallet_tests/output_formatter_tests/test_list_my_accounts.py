import re
from typing import Dict

import test_tools as tt

from .local_tools import verify_json_patterns, verify_text_patterns

BALANCES = {
    'alice': {
        'hives': tt.Asset.Test(100),
        'vests': tt.Asset.Test(110),
        'hbds': tt.Asset.Tbd(120),
    },
    'bob': {
        'hives': tt.Asset.Test(200),
        'vests': tt.Asset.Test(210),
        'hbds': tt.Asset.Tbd(220),
    },
    'carol': {
        'hives': tt.Asset.Test(300),
        'vests': tt.Asset.Test(310),
        'hbds': tt.Asset.Tbd(320),
    }
}

TOTAL_BALANCES = {
    'hives': sum((balance['hives'] for balance in BALANCES.values()), start=tt.Asset.Test(0)),
    'hbds': sum((balance['hbds'] for balance in BALANCES.values()), start=tt.Asset.Tbd(0))
}


def test_list_my_accounts_json_format(wallet_with_json_formatter):
    for name, balances in BALANCES.items():
        wallet_with_json_formatter.create_account(name=name, **balances)

    accounts_summary = wallet_with_json_formatter.api.list_my_accounts()
    for name, balances, returned_account in zip(BALANCES.keys(), BALANCES.values(), accounts_summary['accounts']):
        assert returned_account['name'] == name
        assert returned_account['balance'] == balances['hives'].as_nai()
        assert returned_account['hbd_balance'] == balances['hbds'].as_nai()

    assert accounts_summary['total_hive'] == TOTAL_BALANCES['hives'].as_nai()
    assert accounts_summary['total_hbd'] == TOTAL_BALANCES['hbds'].as_nai()


def test_list_my_accounts_text_format(wallet_with_text_formatter):
    for name, balances in BALANCES.items():
        wallet_with_text_formatter.create_account(name=name, **balances)

    accounts_summary = parse_text_response(wallet_with_text_formatter.api.list_my_accounts())

    for name, balances, returned_account in zip(BALANCES.keys(), BALANCES.values(), accounts_summary['accounts']):
        assert returned_account['name'] == name
        assert returned_account['balance'] == balances['hives']
        assert returned_account['hbd_balance'] == balances['hbds']

    assert accounts_summary['total_hive'] == TOTAL_BALANCES['hives']
    assert accounts_summary['total_hbd'] == TOTAL_BALANCES['hbds']


def test_list_my_accounts_json_format_pattern_comparison(wallet_with_json_formatter):
    for name, balances in BALANCES.items():
        wallet_with_json_formatter.create_account(name=name, **balances)

    accounts_summary = wallet_with_json_formatter.api.list_my_accounts()

    verify_json_patterns('list_my_accounts', accounts_summary)


def test_list_my_accounts_text_format_pattern_comparison(wallet_with_text_formatter):
    for name, balances in BALANCES.items():
        wallet_with_text_formatter.create_account(name=name, **balances)

    accounts_summary = wallet_with_text_formatter.api.list_my_accounts()

    verify_text_patterns('list_my_accounts', accounts_summary)


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
            'total_hive':  splitted_values[1],
            'total_vest': splitted_values[2],
            'total_hbd': splitted_values[3],
        }

    lines = text.splitlines()
    return {
        'accounts': [parse_single_line_with_account_balances(line_to_parse) for line_to_parse in lines[0:3]],
        **parse_single_line_with_total_balances(lines[4]),
    }
