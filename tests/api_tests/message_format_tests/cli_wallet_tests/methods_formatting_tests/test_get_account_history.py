import json
import re
from typing import Dict

from .local_tools import verify_json_patterns, verify_text_patterns


def test_get_account_history(node, wallet_with_json_formatter, wallet_with_text_formatter):
    # Wait to appear transactions in account_history
    node.wait_number_of_blocks(21)

    json = wallet_with_json_formatter.api.get_account_history('initminer', 2, 2)
    text_parsed_to_list = parse_text_response(wallet_with_text_formatter.api.get_account_history('initminer', 2, 2))

    for acc_history_json, acc_history_text in zip(json, text_parsed_to_list):
        assert acc_history_json == acc_history_text


def test_json_format_pattern(node, wallet_with_json_formatter):
    # Wait to appear transactions in account_history
    node.wait_number_of_blocks(21)

    json = wallet_with_json_formatter.api.get_account_history('initminer', 2, 2)

    verify_json_patterns('get_account_history', json)


def test_text_format_pattern(node, wallet_with_text_formatter):
    # Wait to appear transactions in account_history
    node.wait_number_of_blocks(21)

    text = wallet_with_text_formatter.api.get_account_history('initminer', 2, 2)

    verify_text_patterns('get_account_history', text)


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
    return [parse_single_line_with_order_values(line_to_parse) for line_to_parse in lines[2:-1]]
