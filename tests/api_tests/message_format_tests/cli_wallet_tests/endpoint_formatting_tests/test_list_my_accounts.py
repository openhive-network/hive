import re
from typing import Dict

import test_tools as tt

from .local_tools import verify_json_patterns, verify_text_patterns
from .....local_tools import create_account_and_fund_it

BALANCES = {
    'alice': {
        'tests': tt.Asset.Test(100),
        'vests': tt.Asset.Test(110),
        'tbds': tt.Asset.Tbd(120),
    },
    'bob': {
        'tests': tt.Asset.Test(200),
        'vests': tt.Asset.Test(210),
        'tbds': tt.Asset.Tbd(220),
    },
    'carol': {
        'tests': tt.Asset.Test(300),
        'vests': tt.Asset.Test(310),
        'tbds': tt.Asset.Tbd(320),
    }
}

TOTAL_BALANCES = {
    'tests': sum((balance['tests'] for balance in BALANCES.values()), start=tt.Asset.Test(0)),
    'tbds': sum((balance['tbds'] for balance in BALANCES.values()), start=tt.Asset.Tbd(0))
}


def test_list_my_accounts_json_format(wallet_with_json_formatter):
    for name, balances in BALANCES.items():
        create_account_and_fund_it(wallet_with_json_formatter, name=name, **balances)

    accounts_summary = wallet_with_json_formatter.api.list_my_accounts()
    for name, balances, returned_account in zip(BALANCES.keys(), BALANCES.values(), accounts_summary['accounts']):
        assert returned_account['name'] == name
        assert returned_account['balance'] == balances['tests'].as_nai()
        assert returned_account['hbd_balance'] == balances['tbds'].as_nai()

    assert accounts_summary['total_hive'] == TOTAL_BALANCES['tests'].as_nai()
    assert accounts_summary['total_hbd'] == TOTAL_BALANCES['tbds'].as_nai()


def test_list_my_accounts_text_format(wallet_with_text_formatter):
    for name, balances in BALANCES.items():
        create_account_and_fund_it(wallet_with_text_formatter, name=name, **balances)

    accounts_summary = parse_text_response(wallet_with_text_formatter.api.list_my_accounts())

    for name, balances, returned_account in zip(BALANCES.keys(), BALANCES.values(), accounts_summary['accounts']):
        assert returned_account['name'] == name
        assert returned_account['balance'] == balances['tests']
        assert returned_account['hbd_balance'] == balances['tbds']

    assert accounts_summary['total_hive'] == TOTAL_BALANCES['tests']
    assert accounts_summary['total_hbd'] == TOTAL_BALANCES['tbds']


def test_list_my_accounts_json_format_pattern_comparison(wallet_with_json_formatter):
    for name, balances in BALANCES.items():
        create_account_and_fund_it(wallet_with_json_formatter, name=name, **balances)

    accounts_summary = wallet_with_json_formatter.api.list_my_accounts()

    verify_json_patterns('list_my_accounts', accounts_summary)


def test_list_my_accounts_text_format_pattern_comparison(wallet_with_text_formatter):
    for name, balances in BALANCES.items():
        create_account_and_fund_it(wallet_with_text_formatter, name=name, **balances)

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
