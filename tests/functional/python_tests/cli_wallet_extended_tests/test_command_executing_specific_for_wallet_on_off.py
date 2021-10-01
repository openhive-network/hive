import os.path
import re

import pytest

from test_tools import Wallet
from test_tools.exceptions import CommunicationError

from utilities import send_and_assert_result, send_with_args_and_assert_result


@pytest.fixture
def unconfigured_offline_wallet():
    return Wallet(preconfigure=False)


@pytest.fixture
def unconfigured_online_wallet(node):
    return Wallet(attach_to=node, preconfigure=False)


@pytest.fixture
def configured_offline_wallet():
    return Wallet()


@pytest.fixture(params=['unconfigured_online_wallet', 'unconfigured_offline_wallet'])
def unconfigured_wallet(request):
    return request.getfixturevalue(request.param)


@pytest.fixture(params=['wallet', 'configured_offline_wallet'])
def configured_wallet(request):
    return request.getfixturevalue(request.param)


path_to_wallet = '/home/dev/hive/tests/functional/python_tests/cli_wallet_extended_tests/test_wallet.json'


def test_if_state_is_new_after_first_start(unconfigured_wallet: Wallet):
    send_and_assert_result(unconfigured_wallet.api.is_new, True)
    send_and_assert_result(unconfigured_wallet.api.is_locked, True)

def test_if_state_is_locked_after_first_password_set(unconfigured_wallet: Wallet):
    unconfigured_wallet.api.set_password('password')
    send_and_assert_result(unconfigured_wallet.api.is_new, False)
    send_and_assert_result(unconfigured_wallet.api.is_locked, True)

def test_if_state_is_unlocked_after_entering_password(unconfigured_wallet: Wallet):
    unconfigured_wallet.api.set_password('password')
    unconfigured_wallet.api.unlock('password')
    send_and_assert_result(unconfigured_wallet.api.is_new, False)
    send_and_assert_result(unconfigured_wallet.api.is_locked, False)

def test_if_state_is_locked_after_entering_password(unconfigured_wallet: Wallet):
    unconfigured_wallet.api.set_password('password')
    unconfigured_wallet.api.unlock('password')
    send_and_assert_result(unconfigured_wallet.api.lock, None)
    send_and_assert_result(unconfigured_wallet.api.is_new, False)
    send_and_assert_result(unconfigured_wallet.api.is_locked, True)

def test_if_state_is_locked_after_close_and_reopen(unconfigured_wallet: Wallet):
    unconfigured_wallet.api.set_password('password')
    unconfigured_wallet.api.unlock('password')
    unconfigured_wallet.restart(preconfigure=False)
    send_and_assert_result(unconfigured_wallet.api.is_new, False)
    send_and_assert_result(unconfigured_wallet.api.is_locked, True)

def test_save_wallet_to_file(configured_wallet: Wallet):
    if os.path.exists(path_to_wallet) == True:
        os.remove(path_to_wallet)
    send_with_args_and_assert_result(configured_wallet.api.save_wallet_file, path_to_wallet, None)
    assert os.path.exists(path_to_wallet)

def test_load_wallet_from_file(configured_wallet: Wallet):
    configured_wallet.api.save_wallet_file(path_to_wallet)
    send_with_args_and_assert_result(configured_wallet.api.load_wallet_file, path_to_wallet, True)

def test_get_prototype_operation(configured_wallet: Wallet):
    response = configured_wallet.api.get_prototype_operation('comment_operation')
    result = response['result']
    assert 'comment' in result

def test_about(configured_wallet: Wallet):
    response = configured_wallet.api.about()
    result = response['result']
    assert 'blockchain_version' in result
    assert 'client_version' in result

def test_normalize_brain_key(configured_wallet: Wallet):
    send_with_args_and_assert_result(configured_wallet.api.normalize_brain_key, '     mango Apple banana CHERRY ', 'MANGO APPLE BANANA CHERRY')

def test_list_keys_and_import_key(unconfigured_wallet: Wallet):
    unconfigured_wallet.api.set_password('password')
    unconfigured_wallet.api.unlock('password')
    response = unconfigured_wallet.api.list_keys()
    keys = response['result']
    assert len(keys) == 0

    unconfigured_wallet.api.import_key('5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3')
    unconfigured_wallet.api.import_key('5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n')

    response = unconfigured_wallet.api.list_keys()
    keys = response['result']
    assert len(keys) == 2
    assert keys[0][1] == '5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n'
    assert keys[1][1] == '5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3'

def test_generate_privat_key_related_to_account_role_password(configured_wallet: Wallet):
    response = configured_wallet.api.get_private_key_from_password('hulabula', 'owner', 'apricot')
    result = response['result']

    assert len(result) == 2

    assert result[0] == 'TST5Fuu7PnmJh5dxguaxMZU1KLGcmAh8xgg3uGMUmV9m62BDQb3kB'
    assert result[1] == '5HwfhtUXPdxgwukwfjBbwogWfaxrUcrJk6u6oCfv4Uw6DZwqC1H'

def test_generate_private_key_related_to_public_key(configured_wallet: Wallet):
    send_with_args_and_assert_result(configured_wallet.api.get_private_key,
                                     'TST6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4',
                                     '5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n')

def test_help_and_gethelp(configured_wallet: Wallet):
    help_content = configured_wallet.api.help()['result']
    help_functions = [re.match(r'.* ([\w_]+)\(.*', line)[1] for line in help_content.split('\n')[:-1]]  # saparate names of functions from "help"
    error_list = []
    for command in help_functions:
        try:
            configured_wallet.api.gethelp(command)
        except CommunicationError:
            error_list.append(f'Error in command: {command}')
    if len(error_list) > 0:
        print(*error_list, sep="\n")
        assert False, f'Error occurred when gethelp was called for following commands: {error_list}'

def test_suggest_brain_key(configured_wallet: Wallet):
    response = configured_wallet.api.suggest_brain_key()

    result = response['result']
    brain_priv_key = result['brain_priv_key']
    items = brain_priv_key.split(' ')
    assert len(items) == 16

    assert len(result['wif_priv_key']) == 51
    assert result['pub_key'].find('TST') != -1

def test_set_transaction_expiration(configured_wallet: Wallet):
    send_with_args_and_assert_result(configured_wallet.api.set_transaction_expiration, 31, None)

def test_serialize_transaction(configured_wallet: Wallet, node): #impossible to test, transaction does not exist
    wallet_temp = Wallet(attach_to=node)
    transaction = wallet_temp.api.create_account('initminer', 'alice', '{}', False)

    serialized_transaction = configured_wallet.api.serialize_transaction(transaction['result'])
    assert serialized_transaction['result'] != '00000000000000000000000000'

def test_get_encrypted_memo_and_decrypt_memo(configured_wallet: Wallet, node):

    wallet_temp = Wallet(attach_to=node)
    wallet_temp.api.create_account('initminer', 'alice', '{}')
    response = wallet_temp.api.get_encrypted_memo('alice', 'initminer', '#this is memo')
    _encrypted = response['result']
    send_with_args_and_assert_result(configured_wallet.api.decrypt_memo, _encrypted, 'this is memo')





