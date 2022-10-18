import pytest

import test_tools as tt

def prepare_wallets(api_node):
    tt.logger.info( "Attaching legacy/hf26 wallets..." )

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
