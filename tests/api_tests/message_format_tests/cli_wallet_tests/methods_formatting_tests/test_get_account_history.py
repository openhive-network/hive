import json
import re
from typing import Dict

import test_tools as tt

from .local_tools import verify_json_patterns, verify_text_patterns
from .....local_tools import create_account_and_fund_it


def test_get_account_history(node, wallet_with_json_formatter, wallet_with_text_formatter):
    block_num = prepare_transactions(wallet_with_json_formatter)

    # Wait to appear transactions in account_history
    node.wait_number_of_blocks(21)

    account_history_in_json_form = wallet_with_json_formatter.api.get_account_history('alice', block_num[-1] + 1, 2)
    account_history_in_text_form_parsed_to_list = parse_text_response(
        wallet_with_text_formatter.api.get_account_history('alice', block_num[-1] + 1, 2))

    for acc_history_json, acc_history_text in zip(account_history_in_json_form,
                                                  account_history_in_text_form_parsed_to_list):
        assert acc_history_json == acc_history_text


def test_json_format_pattern(node, wallet_with_json_formatter):
    block_num = prepare_transactions(wallet_with_json_formatter)

    # Wait to appear transactions in account_history
    node.wait_number_of_blocks(21)

    account_history_in_json_form = wallet_with_json_formatter.api.get_account_history('alice', block_num[-1] + 1, 2)

    verify_json_patterns('get_account_history', account_history_in_json_form)


def test_text_format_pattern(node, wallet_with_text_formatter):
    block_num = prepare_transactions(wallet_with_text_formatter)

    # Wait to appear transactions in account_history
    node.wait_number_of_blocks(21)

    account_history_in_text_form = wallet_with_text_formatter.api.get_account_history('alice', block_num[-1] + 1, 2)

    verify_text_patterns('get_account_history', account_history_in_text_form)


def parse_text_response(text):
    def parse_single_line_with_order_values(line_to_parse: str) -> Dict:
        splitted_values = re.split(r'\s{2,}', line_to_parse.strip())
        return {
            'pos': int(splitted_values[0]),
            'block': int(splitted_values[1]),
            'id': splitted_values[2],
            'op': {
                'type': f'{splitted_values[3]}_operation',
                'value': json.loads(splitted_values[4]),
            },
        }
    lines = text.splitlines()
    return [parse_single_line_with_order_values(line_to_parse) for line_to_parse in lines[2:]]


def prepare_transactions(wallet) -> list:
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))

    post_comment_transaction =\
        wallet.api.post_comment('alice', 'test-permlink', '', 'someone0', 'test-title', 'this is a body', '{}')
    update_account_auth_key_transaction =\
        wallet.api.update_account_auth_key('alice', 'owner', tt.Account('some key').public_key, 1)

    return [post_comment_transaction['block_num'], update_account_auth_key_transaction['block_num']]
