import json
from pathlib import Path
import re
from typing import Dict

import pytest

from ..local_tools import verify_json_patterns, verify_text_patterns

__PATTERNS_DIRECTORY = Path(__file__).with_name('response_patterns')


@pytest.mark.replayed_node
def test_get_account_history(node, wallet_with_json_formatter, wallet_with_text_formatter):
    # Tested transactions appear in block log on positions 5 and 6.
    account_history_in_json_form = wallet_with_json_formatter.api.get_account_history('alice', 6, 2)
    account_history_in_text_form_parsed_to_list = parse_text_response(
        wallet_with_text_formatter.api.get_account_history('alice', 6, 2))

    assert account_history_in_json_form == account_history_in_text_form_parsed_to_list


@pytest.mark.replayed_node
def test_json_format_pattern(node, wallet_with_json_formatter):
    # Tested transactions appear in block log on positions 5 and 6.
    account_history_in_json_form = wallet_with_json_formatter.api.get_account_history('alice', 6, 2)

    verify_json_patterns(__PATTERNS_DIRECTORY, 'get_account_history', account_history_in_json_form)


@pytest.mark.replayed_node
def test_text_format_pattern(node, wallet_with_text_formatter):
    # Tested transactions appear in block log on positions 5 and 6.
    account_history_in_text_form = wallet_with_text_formatter.api.get_account_history('alice', 6, 2)

    verify_text_patterns(__PATTERNS_DIRECTORY, 'get_account_history', account_history_in_text_form)


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
