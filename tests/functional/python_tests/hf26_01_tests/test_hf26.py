import pytest

import test_tools.exceptions

from test_tools import logger, Wallet, World

def prepare_wallets(world : World):
    api_node      = world.network('Alpha').node('ApiNode0')

    wallet_legacy = Wallet(attach_to=api_node, additional_arguments=['--transaction-serialization=legacy'])
    wallet_hf26   = Wallet(attach_to=api_node, additional_arguments=['--transaction-serialization=hf26'])
    return wallet_legacy, wallet_hf26

def test_before_hf26(world_before_hf26):
    logger.info( "Attaching legacy/hf26 wallets..." )
    wallet_legacy, wallet_hf26 = prepare_wallets(world_before_hf26)

    logger.info( "Creating `legacy` operations..." )
    result = wallet_legacy.api.create_account('initminer', 'alice', '{}')
    result = wallet_legacy.api.transfer('initminer', 'alice', '10.000 TESTS', '', 'true')

    logger.info( "Creating `hf26` operations..." )
    with pytest.raises(test_tools.exceptions.CommunicationError) as exception:
      wallet_hf26.api.transfer('initminer', 'alice', { "amount": "200", "precision": 3, "nai": "@@000000021" }, '', 'true')

    assert exception.value.response['error']['message'].find('missing required active authority'), "Incorrect error in `hf26` transfer operation"

def test_after_hf26(world_after_hf26):
    logger.info( "Attaching legacy/hf26 wallets..." )
    wallet_legacy, wallet_hf26 = prepare_wallets(world_after_hf26)

    logger.info( "Creating `legacy` operations..." )
    result = wallet_legacy.api.create_account('initminer', 'alice', '{}')
    result = wallet_legacy.api.transfer('initminer', 'alice', '10.000 TESTS', '', 'true')

    logger.info( "Creating `hf26` operations..." )
    wallet_hf26.api.transfer('initminer', 'alice', { "amount": "200", "precision": 3, "nai": "@@000000021" }, '', 'true')
