import pytest

import test_tools as tt

from .....local_tools import create_account_and_fund_it


@pytest.mark.testnet
def test_is_legacy_keyword_TESTS_is_not_in_hf26_transaction(node, wallet_with_hf26_serialization, request):
    create_and_power_account_and_update_witness(wallet_with_hf26_serialization)

    path = get_path_to_binary_transaction_stored_from_wallet(wallet_with_hf26_serialization, request)
    bin_transaction = read_binary_file_from_store(path)

    # Check is in transaction appear keyword 'TESTS', which means a currency in legacy format.
    assert bin_transaction.find(b'TESTS') == -1


@pytest.mark.testnet
def test_is_legacy_keyword_TESTS_is_in_legacy_transaction(node, wallet_with_legacy_serialization, request):
    create_and_power_account_and_update_witness(wallet_with_legacy_serialization)

    path = get_path_to_binary_transaction_stored_from_wallet(wallet_with_legacy_serialization, request)
    bin_transaction = read_binary_file_from_store(path)

    # Check is in transaction appear keyword 'TESTS', which means a currency in legacy format.
    assert bin_transaction.find(b'TESTS') != -1


@pytest.mark.testnet
def test_update_witness_serialization(node, wallet, request):
    create_account_and_fund_it(wallet, 'alice', vests=tt.Asset.Test(100))
    update_witness = wallet.api.update_witness('alice', 'http://url.html', tt.Account('alice').public_key,
                                               {'account_creation_fee': tt.Asset.Test(10)})

    path = get_path_to_binary_transaction_stored_from_wallet(wallet, request)
    stored_transaction = read_binary_file_from_store(path).hex()

    update_witness_block_num = update_witness['block_num']
    get_block = wallet.api.get_block(update_witness_block_num)
    transaction_from_get_block = wallet.api.serialize_transaction(get_block['transactions'][0])

    # To show, is update account operation is correctly pack and unpack is created comparison of transaction
    # directly after broadcast with transaction from get_block (after pack and unpack).

    # Stored transaction has a few elements added to the end, but it has no effect on the test, so it was checked
    # if the transaction_from_get_block is included in the beginning of stored_transaction.
    assert stored_transaction.startswith(transaction_from_get_block)


def create_and_power_account_and_update_witness(wallet):
    create_account_and_fund_it(wallet, 'alice', vests=tt.Asset.Test(100))

    wallet.api.update_witness('alice', 'http://url.html', tt.Account('alice').public_key,
                              {'account_creation_fee': tt.Asset.Test(10)})


def get_path_to_binary_transaction_stored_from_wallet(wallet, request):
    return wallet.directory / f'{request.fspath.purebasename}.bin'


def read_binary_file_from_store(path):
    with open(path, 'rb') as file:
        bin_transaction = file.read()

    return bin_transaction
