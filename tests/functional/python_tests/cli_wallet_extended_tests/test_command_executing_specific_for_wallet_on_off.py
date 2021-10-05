from datetime import datetime, timezone, timedelta
import re

import pytest

from test_tools import Account, context, Wallet
from test_tools.exceptions import CommunicationError

from utilities import result_of


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


def test_if_state_is_new_after_first_start(unconfigured_wallet: Wallet):
    assert result_of(unconfigured_wallet.api.is_new) is True
    assert result_of(unconfigured_wallet.api.is_locked) is True

def test_if_state_is_locked_after_first_password_set(unconfigured_wallet: Wallet):
    unconfigured_wallet.api.set_password(unconfigured_wallet.DEFAULT_PASSWORD)
    assert result_of(unconfigured_wallet.api.is_new) is False
    assert result_of(unconfigured_wallet.api.is_locked) is True

def test_if_state_is_unlocked_after_entering_password(unconfigured_wallet: Wallet):
    unconfigured_wallet.api.set_password(unconfigured_wallet.DEFAULT_PASSWORD)
    unconfigured_wallet.api.unlock(unconfigured_wallet.DEFAULT_PASSWORD)
    assert result_of(unconfigured_wallet.api.is_new) is False
    assert result_of(unconfigured_wallet.api.is_locked) is False

def test_if_state_is_locked_after_entering_password(unconfigured_wallet: Wallet):
    unconfigured_wallet.api.set_password(unconfigured_wallet.DEFAULT_PASSWORD)
    unconfigured_wallet.api.unlock(unconfigured_wallet.DEFAULT_PASSWORD)
    unconfigured_wallet.api.lock()
    assert result_of(unconfigured_wallet.api.is_new) is False
    assert result_of(unconfigured_wallet.api.is_locked) is True

def test_if_state_is_locked_after_close_and_reopen(unconfigured_wallet: Wallet):
    unconfigured_wallet.api.set_password(unconfigured_wallet.DEFAULT_PASSWORD)
    unconfigured_wallet.api.unlock(unconfigured_wallet.DEFAULT_PASSWORD)
    unconfigured_wallet.restart(preconfigure=False)
    assert result_of(unconfigured_wallet.api.is_new) is False
    assert result_of(unconfigured_wallet.api.is_locked) is True

def test_save_wallet_to_file(configured_wallet: Wallet):
    wallet_file_path = context.get_current_directory() / 'test_file.json'
    configured_wallet.api.save_wallet_file(str(wallet_file_path))
    assert wallet_file_path.exists()

def test_load_wallet_from_file(configured_wallet: Wallet):
    wallet_file_path = context.get_current_directory() / 'test_file.json'
    configured_wallet.api.save_wallet_file(str(wallet_file_path))
    assert result_of(configured_wallet.api.load_wallet_file, str(wallet_file_path)) is True

def test_get_prototype_operation(configured_wallet: Wallet):
    assert 'comment' in result_of(configured_wallet.api.get_prototype_operation, 'comment_operation')

def test_about(configured_wallet: Wallet):
    assert 'blockchain_version' in result_of(configured_wallet.api.about)
    assert 'client_version' in result_of(configured_wallet.api.about)

def test_normalize_brain_key(configured_wallet: Wallet):
    assert result_of(configured_wallet.api.normalize_brain_key, '     mango Apple banana CHERRY ') == 'MANGO APPLE BANANA CHERRY'

def test_list_keys_and_import_key(unconfigured_wallet: Wallet):
    unconfigured_wallet.api.set_password(unconfigured_wallet.DEFAULT_PASSWORD)
    unconfigured_wallet.api.unlock(unconfigured_wallet.DEFAULT_PASSWORD)
    response = unconfigured_wallet.api.list_keys()
    keys = response['result']
    assert len(keys) == 0

    unconfigured_wallet.api.import_key(Account('initminer').private_key)
    unconfigured_wallet.api.import_key(Account('alice').private_key)

    response = unconfigured_wallet.api.list_keys()
    keys = response['result']
    assert len(keys) == 2
    assert keys[0][1] == Account('alice').private_key
    assert keys[1][1] == Account('initminer').private_key

def test_generate_keys(configured_wallet: Wallet):
    response = result_of(configured_wallet.api.get_private_key_from_password, 'hulabula', 'owner', 'apricot')
    assert len(response) == 2

    assert response[0] == 'TST5Fuu7PnmJh5dxguaxMZU1KLGcmAh8xgg3uGMUmV9m62BDQb3kB'
    assert response[1] == '5HwfhtUXPdxgwukwfjBbwogWfaxrUcrJk6u6oCfv4Uw6DZwqC1H'

def test_get_private_key_related_to_public_key(configured_wallet: Wallet):
    assert result_of(configured_wallet.api.get_private_key, Account('initminer').public_key) == Account('initminer').private_key

def test_help_and_gethelp(configured_wallet: Wallet):
    help_content = result_of(configured_wallet.api.help)
    help_functions = [re.match(r'.* ([\w_]+)\(.*', line)[1] for line in help_content.split('\n')[:-1]]  # saparate names of functions from "help"
    failed_commands = []
    for function in help_functions:
        try:
            configured_wallet.api.gethelp(function)
        except CommunicationError:
            failed_commands.append(function)
    if len(failed_commands) > 0:
        print(*failed_commands, sep="\n")
        assert False, f'Error occurred when gethelp was called for following functions: {failed_commands}'

def test_suggest_brain_key(configured_wallet: Wallet):
    response = result_of(configured_wallet.api.suggest_brain_key)
    brain_priv_key = response['brain_priv_key'].split(' ')

    assert len(brain_priv_key) == 16
    assert len(response['wif_priv_key']) == 51
    assert response['pub_key'].startswith('TST')

def test_set_transaction_expiration(wallet: Wallet):
    set_expiration_time = 1000
    wallet.api.set_transaction_expiration(set_expiration_time)
    transaction = result_of(wallet.api.create_account, 'initminer', 'alice', '{}', False)

    expiration_time_point = datetime.strptime(transaction['expiration'], '%Y-%m-%dT%H:%M:%S')
    expiration_time_point = expiration_time_point.replace(tzinfo=timezone.utc)
    expiration_time = expiration_time_point - datetime.now(timezone.utc)

    block_time = 3
    tolerance = 1
    max_expiration_time = timedelta(seconds=(set_expiration_time + tolerance))
    min_expiration_time = timedelta(seconds=(set_expiration_time - block_time - tolerance))
    assert min_expiration_time <= expiration_time <= max_expiration_time

def test_serialize_transaction(configured_wallet: Wallet, node):
    wallet_temp = Wallet(attach_to=node)
    transaction = wallet_temp.api.create_account('initminer', 'alice', '{}', False)
    serialized_transaction = configured_wallet.api.serialize_transaction(transaction['result'])
    assert serialized_transaction['result'] != '00000000000000000000000000'

def test_get_encrypted_memo_and_decrypt_memo(configured_wallet: Wallet, node):
    wallet_temp = Wallet(attach_to=node)
    wallet_temp.api.create_account('initminer', 'alice', '{}')
    encrypted = result_of(wallet_temp.api.get_encrypted_memo, 'alice', 'initminer', '#this is memo')
    assert result_of(configured_wallet.api.decrypt_memo, encrypted) == 'this is memo'
    
