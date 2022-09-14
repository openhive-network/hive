from .local_tools import verify_json_patterns, verify_text_patterns


def test_help(node, wallet_with_json_formatter, wallet_with_text_formatter):
    help_content_in_json_form = wallet_with_json_formatter.api.help()
    help_content_in_text_form_parsed_to_list = parse_text_response(wallet_with_text_formatter.api.help())

    assert help_content_in_json_form == help_content_in_text_form_parsed_to_list


def test_json_format_pattern(node, wallet_with_json_formatter):
    help_content_in_json_form = wallet_with_json_formatter.api.help()

    verify_json_patterns('help', help_content_in_json_form)


def test_text_format_pattern(node, wallet_with_text_formatter):
    help_content_in_text_form = wallet_with_text_formatter.api.help()

    verify_text_patterns('help', help_content_in_text_form)

def parse_text_response(text):
    return [line for line in text.split('\n') if line != '']
