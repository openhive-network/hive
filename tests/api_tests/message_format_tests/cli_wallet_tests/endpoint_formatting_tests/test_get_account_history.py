import json

from .local_tools import verify_json_patterns, verify_text_patterns, split_text_to_separated_words


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
    words = split_text_to_separated_words(text)
    account_history = []
    for line in words[2:-1]:
        account_history.append({
            'pos': int(line[0]),
            'block': int(line[1]),
            'id': line[2],
            'op': {
                'type': f'{line[3]}_operation',
                'value': json.loads(line[4]),
            },
        })
    return account_history
