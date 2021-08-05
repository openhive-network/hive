from test_tools import Account, logger, World, Asset
import os.path
from utilities import send_and_assert_result, send_with_args_and_assert_result

def test_wallet(wallet):

    pswd = 'pear_peach'

    internal_path = 'generated_during_test_command_executing_specific_for_wallet/test_wallet/InitNodeWallet0/'
    wallet_content_file_name = 'test_wallet.json'

    send_with_args_and_assert_result(wallet.api.save_wallet_file, wallet_content_file_name, None)

    assert os.path.isfile(internal_path + wallet_content_file_name)

    send_with_args_and_assert_result(wallet.api.save_wallet_file, internal_path + wallet_content_file_name, None)

    send_and_assert_result(wallet.api.is_new, False)

    send_and_assert_result(wallet.api.is_locked, False)

    send_with_args_and_assert_result(wallet.api.set_password, pswd, None)

    send_and_assert_result(wallet.api.is_locked, True)

    send_with_args_and_assert_result(wallet.api.unlock, pswd, None)

    send_and_assert_result(wallet.api.is_locked, False)

    send_and_assert_result(wallet.api.lock, None)

    send_and_assert_result(wallet.api.is_locked, True)

    send_with_args_and_assert_result(wallet.api.unlock, pswd, None)

    send_and_assert_result(wallet.api.is_locked, False)

    response = wallet.api.list_keys()

    _keys = response['result'][0]
    assert len(_keys) == 2
    assert _keys[0] == 'TST6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4'
    assert _keys[1] == '5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n'

    wallet.api.import_key('5JE4eBgPiRiVcdcpJ8tQpMpm6dgm1uAuq9Kn2nn1M9xK94EE5nU')

    response = wallet.api.list_keys()
    _result = response['result']

    assert len(_result) == 2
    _keys = _result[1]

    assert len(_keys) == 2
    assert _keys[0] == 'TST8LRQW8NXuDnextVpzKA5Fp47k591mHJjKnfTMdfQvjR5qA1yqK'
    assert _keys[1] == '5JE4eBgPiRiVcdcpJ8tQpMpm6dgm1uAuq9Kn2nn1M9xK94EE5nU'

    response = wallet.api.get_private_key_from_password('hulabula', 'owner', "apricot")
    _result = response['result']

    assert len(_result) == 2

    assert _result[0] == 'TST5Fuu7PnmJh5dxguaxMZU1KLGcmAh8xgg3uGMUmV9m62BDQb3kB'
    assert _result[1] == '5HwfhtUXPdxgwukwfjBbwogWfaxrUcrJk6u6oCfv4Uw6DZwqC1H'

    send_with_args_and_assert_result(wallet.api.get_private_key, 'TST6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4', '5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n')

    response = wallet.api.gethelp('find_proposals')

    found = response['result'].find('Find proposal with given id')
    assert found != -1

    response = wallet.api.gethelp('create_proposal')

    found = response['result'].find('Create worker proposal')
    assert found != -1

    response = wallet.api.help()

    _result = response['result']
    val = 'serializer_wrapper<annotated_signed_transaction> cancel_transfer_from_savings(const string & from, uint32_t request_id, bool broadcast)'
    found = _result.find(val)
    assert found != -1

    val = 'serializer_wrapper<annotated_signed_transaction> convert_hbd(const string & from, const serializer_wrapper<hive::protocol::asset> & amount, bool broadcast)'
    found = _result.find(val)
    assert found != -1

    response = wallet.api.info()

    _result = response['result']
    assert 'median_hbd_price' in _result
    _median_hbd_price = _result['median_hbd_price']

    assert _median_hbd_price['base'] == Asset.Tbd(0.001)
    assert _median_hbd_price['quote'] == Asset.Test(0.001)

    assert _result['account_creation_fee'] == Asset.Test(0)

    _post_reward_fund = _result['post_reward_fund']
    assert _post_reward_fund['reward_balance'] != Asset.Test(0)

    send_with_args_and_assert_result(wallet.api.normalize_brain_key, '     mango apple banana CHERRY ', 'MANGO APPLE BANANA CHERRY')

    response = wallet.api.suggest_brain_key()

    _result = response['result']
    _brain_priv_key = _result['brain_priv_key']
    _items = _brain_priv_key.split(' ')
    assert len(_items) == 16

    assert len(_result['wif_priv_key']) == 51
    assert _result['pub_key'].find('TST') != -1

    response = wallet.api.about()

    _result = response['result']
    assert 'blockchain_version' in _result
    assert 'client_version' in _result
    assert 'hive_revision' in _result
    assert 'hive_revision_age' in _result
    assert 'fc_revision' in _result
    assert 'fc_revision_age' in _result
    assert 'compile_date' in _result
    assert 'boost_version' in _result
    assert 'openssl_version' in _result
    assert 'build' in _result
    assert 'server_blockchain_version' in _result
    assert 'server_hive_revision' in _result
    assert 'server_fc_revision' in _result
