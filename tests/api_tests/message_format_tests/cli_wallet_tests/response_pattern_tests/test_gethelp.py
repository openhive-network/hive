from pathlib import Path

import pytest

import test_tools as tt


@pytest.fixture
def node() -> tt.InitNode:
    node = tt.InitNode()
    node.run()
    return node


@pytest.fixture
def wallet(node) -> tt.Wallet:
    return tt.Wallet(attach_to=node)


__PATTERNS_DIRECTORY = Path(__file__).with_name('gethelp_response_patterns')


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
