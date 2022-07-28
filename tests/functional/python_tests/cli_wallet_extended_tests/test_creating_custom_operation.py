import pytest

import test_tools as tt


def test_creating_custom_operation(node, wallet):
    custom_operation_data_in_hex = '0a627974'
    transaction = wallet.api.custom(["initminer"], 100, custom_operation_data_in_hex)

    # waiting 22 blocks for the block with the transaction to become irreversible
    node.wait_number_of_blocks(22)
    assert transaction != node.api.account_history.get_transaction(id=transaction['transaction_id'])

    CUSTOM_OPERATION_DATA_MAX_LENGTH = node.api.database.get_config()['HIVE_CUSTOM_OP_DATA_MAX_LENGTH']
    MAX_AUTHORITY_MEMBERSHIP = node.api.database.get_config()['HIVE_MAX_AUTHORITY_MEMBERSHIP']

    '''
       Try to send data over CUSTOM_OPERATION_DATA_MAX_LENGTH from Hived config. Multiply max value 2 times because 2
       characters in hex is 1 byte. In this case 8192 * 2 * 0,5 of byte = 8192 bytes. To exeed this numer over 1 byte we 
       have to add 2 more characters.
    '''
    with pytest.raises(tt.exceptions.CommunicationError):
        wallet.api.custom(['initminer'], 101, ((CUSTOM_OPERATION_DATA_MAX_LENGTH*2)+2)*'1')

    auths = ['initminer', 'hive.fund', 'null']
    with wallet.in_single_transaction():
        for i in range(MAX_AUTHORITY_MEMBERSHIP):
            wallet.api.create_account('initminer', f'account-{i}', '{}')
            auths.append(f'account-{i}')

    # try to use more required_auths than MAX_AUTHORITY_MEMBERSHIP from Hived config
    with pytest.raises(tt.exceptions.CommunicationError):
        wallet.api.custom(auths, 102, custom_operation_data_in_hex)
