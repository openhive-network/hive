import pytest

from test_tools import Account, Asset, exceptions, logger, Wallet

from validate_message import validate_message
import schemas

# TODO BUG LIST!
'''
1. Problem with run command wallet_bridge_api.list_witnesses() with incorrect type of limit argument. 
   Exception is not throw (#BUG1)
'''

WITNESSES_NAMES = [f'witness-{i}' for i in range(21)]


@pytest.mark.parametrize(
    'witness_account', [
        WITNESSES_NAMES[0],
        WITNESSES_NAMES[-1],
        'non-exist-acc',
    ],
)
def test_get_witness_with_correct_value(world, witness_account):
    # TODO Add pattern test.
    node = prepare_node_with_witnesses(world)
    response = node.api.wallet_bridge.get_witness(witness_account)


@pytest.mark.parametrize(
    'witness_account', [
        100,
        True
    ]
)
def test_get_witness_with_incorrect_type_of_argument(node, witness_account):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_witness(witness_account)


def test_get_active_witnesses_with_correct_value(world):
    # TODO Add pattern test
    node = prepare_node_with_witnesses(world)
    response = node.api.wallet_bridge.get_active_witnesses()


def test_get_witness_schedule_with_correct_value(world):
    # TODO Add pattern test
    node = prepare_node_with_witnesses(world)
    response = node.api.wallet_bridge.get_witness_schedule()

    validate_message(
        node.api.wallet_bridge.get_witness_schedule(),
        schemas.get_witnesses_schedule,
    )


@pytest.mark.parametrize(
    'witness_account, limit', [
        # WITNESS ACCOUNT
        (WITNESSES_NAMES[0], 100),
        (WITNESSES_NAMES[20], 100),
        ('non-exist-acc', 100),

        # LIMIT
        (WITNESSES_NAMES[0], 0),
        (WITNESSES_NAMES[0], 1000),
    ]
)
def test_list_witnesses_with_correct_value(world, witness_account, limit):
    # TODO Add pattern test
    node = prepare_node_with_witnesses(world)
    response = node.api.wallet_bridge.list_witnesses(witness_account, limit)

    validate_message(
        node.api.wallet_bridge.list_witnesses(witness_account, limit),
        schemas.list_witnesses,
    )


@pytest.mark.parametrize(
    'witness_account, limit', [
        # LIMIT
        (WITNESSES_NAMES[0], -1),
        (WITNESSES_NAMES[0], 1001),
    ]
)
def test_list_witnesses_with_incorrect_value(world, witness_account, limit):
    node = prepare_node_with_witnesses(world)
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.list_witnesses(witness_account, limit)


@pytest.mark.parametrize(
    'witness_account, limit', [
        # WITNESS ACCOUNT
        (100, 100),
        (True, 100),

        # LIMIT
        (WITNESSES_NAMES[0], 'incorrect_string_argument'),
        (WITNESSES_NAMES[0], True),  # Exception is not throw
    ]
)
def test_list_witnesses_with_incorrect_argument_type(world, witness_account, limit):
    node = prepare_node_with_witnesses(world)
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.list_witnesses(witness_account, limit)


def prepare_node_with_witnesses(world):
    node = world.create_init_node()
    for name in WITNESSES_NAMES:
        witness = Account(name)
        node.config.witness.append(witness.name)
        node.config.private_key.append(witness.private_key)

    node.run()
    wallet = Wallet(attach_to=node)

    with wallet.in_single_transaction():
        for name in WITNESSES_NAMES:
            wallet.api.create_account('initminer', name, '')

    with wallet.in_single_transaction():
        for name in WITNESSES_NAMES:
            wallet.api.transfer_to_vesting("initminer", name, Asset.Test(1000))

    with wallet.in_single_transaction():
        for name in WITNESSES_NAMES:
            wallet.api.update_witness(
                name, "https://" + name,
                Account(name).public_key,
                {"account_creation_fee": Asset.Test(3), "maximum_block_size": 65536, "sbd_interest_rate": 0}
            )

    logger.info('Wait 22 block, to next schedule of witnesses.')
    node.wait_number_of_blocks(22)

    return node
