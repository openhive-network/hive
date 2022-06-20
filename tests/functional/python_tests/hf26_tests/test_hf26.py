import pytest

import test_tools as tt

def prepare_wallets(prepared_networks, api_node_name):
    api_node      = prepared_networks['alpha'].node(api_node_name)

    wallet_legacy = tt.Wallet(attach_to=api_node, additional_arguments=['--transaction-serialization=legacy'])
    wallet_hf26   = tt.Wallet(attach_to=api_node, additional_arguments=['--transaction-serialization=hf26'])
    return wallet_legacy, wallet_hf26

def legacy_operation_passed(wallet):
    tt.logger.info( "Creating `legacy` operations (pass expected)..." )
    result = wallet.api.create_account('initminer', 'alice', '{}')
    result = wallet.api.transfer('initminer', 'alice', '10.123 TESTS', '', 'true')

def hf26_operation_passed(wallet):
    tt.logger.info( "Creating `hf26` operations (pass expected)..." )
    wallet.api.transfer('initminer', 'alice', { "amount": "199", "precision": 3, "nai": "@@000000021" }, '', 'true')

def hf26_operation_failed(wallet):
    tt.logger.info( "Creating `hf26` operations (fail expected)..." )

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
      wallet.api.transfer('initminer', 'alice', { "amount": "200", "precision": 3, "nai": "@@000000021" }, '', 'true')

    assert exception.value.response['error']['message'].find('missing required active authority'), "Incorrect error in `hf26` transfer operation"

def test_before_hf26(network_before_hf26):
    tt.logger.info( "Attaching legacy/hf26 wallets..." )

    wallet_legacy, wallet_hf26 = prepare_wallets(network_before_hf26, 'ApiNode0')

    legacy_operation_passed(wallet_legacy)
    hf26_operation_failed(wallet_hf26)

def test_after_hf26(network_after_hf26):
    tt.logger.info( "Attaching legacy/hf26 wallets..." )

    wallet_legacy, wallet_hf26 = prepare_wallets(network_after_hf26, 'ApiNode1')

    legacy_operation_passed(wallet_legacy)
    hf26_operation_passed(wallet_hf26)

def test_after_hf26_without_majority(network_after_hf26_without_majority):
    tt.logger.info( "Attaching legacy/hf26 wallets..." )

    wallet_legacy, wallet_hf26 = prepare_wallets(network_after_hf26_without_majority, 'ApiNode2')

    legacy_operation_passed(wallet_legacy)
    hf26_operation_failed(wallet_hf26)
