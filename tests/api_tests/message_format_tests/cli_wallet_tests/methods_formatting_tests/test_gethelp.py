from .local_tools import verify_json_patterns, verify_text_patterns


def test_gethelp(node, wallet_with_json_formatter, wallet_with_text_formatter):
    json = wallet_with_json_formatter.api.gethelp('list_my_accounts')
    text = parse_text_response(wallet_with_text_formatter.api.gethelp('list_my_accounts'))
    assert json == text


def test_json_format_pattern(node, wallet_with_json_formatter):
    json = wallet_with_json_formatter.api.gethelp('list_my_accounts')

    verify_json_patterns('gethelp', json)


def test_text_format_pattern(node, wallet_with_text_formatter):
    text = wallet_with_text_formatter.api.gethelp('list_my_accounts')

    verify_text_patterns('gethelp', text)


def parse_text_response(text):
    return [line for line in text.split('\n') if line != '']
