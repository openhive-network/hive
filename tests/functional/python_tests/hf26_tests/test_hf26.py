import pytest

import test_tools.exceptions

from test_tools import logger, Wallet, World

def prepare_wallets(network_number : int, world : World):
    api_node      = world.network(f'alpha-{network_number}').node('ApiNode0')

    wallet_legacy = Wallet(attach_to=api_node, additional_arguments=['--transaction-serialization=legacy'])
    wallet_hf26   = Wallet(attach_to=api_node, additional_arguments=['--transaction-serialization=hf26'])
    return wallet_legacy, wallet_hf26

def legacy_operation_passed(wallet : Wallet):
    logger.info( "Creating `legacy` operations (pass expected)..." )
    result = wallet.api.create_account('initminer', 'alice', '{}')
    result = wallet.api.transfer('initminer', 'alice', '10.123 TESTS', '', 'true')

def hf26_operation_passed(wallet : Wallet):
    logger.info( "Creating `hf26` operations (pass expected)..." )
    wallet.api.transfer('initminer', 'alice', { "amount": "199", "precision": 3, "nai": "@@000000021" }, '', 'true')

def hf26_operation_failed(wallet : Wallet):
    logger.info( "Creating `hf26` operations (fail expected)..." )

    with pytest.raises(test_tools.exceptions.CommunicationError) as exception:
      wallet.api.transfer('initminer', 'alice', { "amount": "200", "precision": 3, "nai": "@@000000021" }, '', 'true')

    assert exception.value.response['error']['message'].find('missing required active authority'), "Incorrect error in `hf26` transfer operation"

def test_before_hf26(world_before_hf26):
    logger.info( "Attaching legacy/hf26 wallets..." )

    #for debug purposes
    network_number = 0

    wallet_legacy, wallet_hf26 = prepare_wallets(network_number, world_before_hf26)

    legacy_operation_passed(wallet_legacy)
    hf26_operation_failed(wallet_hf26)

def test_after_hf26(world_after_hf26):
    logger.info( "Attaching legacy/hf26 wallets..." )

    #for debug purposes
    network_number = 1

    wallet_legacy, wallet_hf26 = prepare_wallets(network_number, world_after_hf26)

    legacy_operation_passed(wallet_legacy)
    hf26_operation_passed(wallet_hf26)

def test_after_hf26_without_majority(world_after_hf26_without_majority):
    logger.info( "Attaching legacy/hf26 wallets..." )

    #for debug purposes
    network_number = 2

    wallet_legacy, wallet_hf26 = prepare_wallets(network_number, world_after_hf26_without_majority)

    legacy_operation_passed(wallet_legacy)
    hf26_operation_failed(wallet_hf26)
