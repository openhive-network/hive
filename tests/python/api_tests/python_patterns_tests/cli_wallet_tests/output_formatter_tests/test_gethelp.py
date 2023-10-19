from __future__ import annotations

from pathlib import Path

from hive_local_tools.api.message_format.cli_wallet import verify_json_patterns, verify_text_patterns

__PATTERNS_DIRECTORY = Path(__file__).with_name("response_patterns")


def test_gethelp(node, wallet_with_json_formatter, wallet_with_text_formatter):
    method_documentation_in_json_form = wallet_with_json_formatter.api.gethelp("list_my_accounts")
    method_documentation_in_text_form = parse_text_response(wallet_with_text_formatter.api.gethelp("list_my_accounts"))

    assert method_documentation_in_json_form == method_documentation_in_text_form


def test_json_format_pattern(node, wallet_with_json_formatter):
    method_documentation_in_json_form = wallet_with_json_formatter.api.gethelp("list_my_accounts")

    verify_json_patterns(__PATTERNS_DIRECTORY, "gethelp", method_documentation_in_json_form)


def test_text_format_pattern(node, wallet_with_text_formatter):
    method_documentation_in_text_form = wallet_with_text_formatter.api.gethelp("list_my_accounts")

    verify_text_patterns(__PATTERNS_DIRECTORY, "gethelp", method_documentation_in_text_form)


def parse_text_response(text):
    return [line for line in text.splitlines() if line != ""]
