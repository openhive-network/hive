import pytest

import test_tools as tt

def prepare_wallets(prepared_networks):
    tt.logger.info( "Attaching legacy/hf26 wallets..." )

    api_node      = prepared_networks['alpha'].node('ApiNode0')

    wallet_legacy = tt.Wallet(attach_to=api_node, additional_arguments=['--transaction-serialization=legacy'])
    wallet_hf26   = tt.Wallet(attach_to=api_node, additional_arguments=['--transaction-serialization=hf26'])
    return wallet_legacy, wallet_hf26

def legacy_operation_passed(wallet):
    tt.logger.info( "Creating `legacy` operations (pass expected)..." )
    wallet.api.create_account('initminer', 'alice', '{}')
    wallet.api.transfer('initminer', 'alice', str(tt.Asset.Test(10123)), 'memo')

def hf26_operation_passed(wallet):
    tt.logger.info( "Creating `hf26` operations (pass expected)..." )
    wallet.api.transfer('initminer', 'alice', tt.Asset.Test(199).as_nai(), 'memo')

def hf26_operation_failed(wallet):
    tt.logger.info( "Creating `hf26` operations (fail expected)..." )

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
      wallet.api.transfer('initminer', 'alice', tt.Asset.Test(200).as_nai(), 'memo')

    assert 'missing required active authority' in exception.value.response['error']['message'], "Incorrect error in `hf26` transfer operation"

def test_before_hf26(network_before_hf26):
    wallet_legacy, wallet_hf26 = prepare_wallets(network_before_hf26)

    legacy_operation_passed(wallet_legacy)
    hf26_operation_failed(wallet_hf26)

def test_after_hf26(network_after_hf26):
    wallet_legacy, wallet_hf26 = prepare_wallets(network_after_hf26)

    legacy_operation_passed(wallet_legacy)
    hf26_operation_passed(wallet_hf26)

def test_after_hf26_without_majority(network_after_hf26_without_majority):
    wallet_legacy, wallet_hf26 = prepare_wallets(network_after_hf26_without_majority)

    legacy_operation_passed(wallet_legacy)
    hf26_operation_failed(wallet_hf26)
