from pathlib import Path
import re

import pytest

import test_tools as tt

from hive_local_tools.api.message_format.cli_wallet import verify_text_patterns


__PATTERNS_DIRECTORY = Path(__file__).with_name('gethelp_response_patterns')

__HELP_CONTENT_DIRECTORY = Path(__file__).with_name('help_response_patterns')


def __read_and_parse_help_content() -> list:
    with open(f'{__HELP_CONTENT_DIRECTORY}/help.pat.txt', 'r') as text_file:
        help_content = text_file.read()
        # saparate names of functions from "help"
        return [re.match(r'.* ([\w_]+)\(.*', line)[1] for line in help_content.split('\n')[:-1]]


@pytest.mark.parametrize(
    'method', __read_and_parse_help_content()
)
def test_gethelp(wallet: tt.Wallet, method):
    response = wallet.api.gethelp(method)
    verify_text_patterns(__PATTERNS_DIRECTORY, method, response)
