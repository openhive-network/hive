from pathlib import Path
import re

import test_tools as tt


__PATTERNS_DIRECTORY = Path(__file__).with_name('gethelp_response_patterns')

__HELP_CONTENT_DIRECTORY = Path(__file__).with_name('help_response_patterns')


def test_gethelp(wallet: tt.Wallet):
    help_functions = __read_and_parse_help_content()
    failed_functions = []
    for function in help_functions:
        try:
            wallet.api.gethelp(function)
        except tt.exceptions.CommunicationError:
            failed_functions.append(function)
    if len(failed_functions) > 0:
        assert False, f'Error occurred when gethelp was called for following functions: {failed_functions}'

def test_gethelp_with_get_order_book_argument(wallet):
    response = wallet.api.gethelp('get_order_book')
    response_pattern = __read_from_text_pattern('get_order_book')

    assert response == response_pattern


def test_gethelp_with_create_account_argument(wallet):
    response = wallet.api.gethelp('create_account')
    response_pattern = __read_from_text_pattern('create_account')

    assert response == response_pattern


def __read_from_text_pattern(method_name: str) -> str:
    with open(f'{__PATTERNS_DIRECTORY}/{method_name}.pat.txt', 'r') as text_file:
        return text_file.read()


def __read_and_parse_help_content() -> list:
    with open(f'{__HELP_CONTENT_DIRECTORY}/help.pat.txt', 'r') as text_file:
        help_content = text_file.read()
        # saparate names of functions from "help"
        return [re.match(r'.* ([\w_]+)\(.*', line)[1] for line in help_content.split('\n')[:-1]]
