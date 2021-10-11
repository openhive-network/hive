from utilities import create_accounts

def test_direct_rc_delegations(wallet):
    creator = 'initminer'
    delegator = 'delegator'
    receiver = 'receiver'
    create_accounts( wallet, 'initminer', [delegator, receiver] )
    wallet.api.transfer(creator, receiver, '10.000 TESTS', '', 'true')
    wallet.api.transfer_to_vesting(creator, delegator, '0.010 TESTS', 'true')
    wallet.api.transfer(creator, receiver, '1.000 TESTS', '', 'true')
    try:
        wallet.api.transfer(receiver, receiver, '0.001 TESTS', '', 'true')
    except Exception as e:
        message = str(e.response)
        found = message.find('receiver has 0 RC, needs 4 RC. Please wait to transact')
        assert found != -1
    rc_receiver = wallet.api.find_rc_accounts([receiver])['result'][0]
    rc_delegator = wallet.api.find_rc_accounts([delegator])['result'][0]
    rc_delegator_before = rc_delegator

    assert (rc_receiver['delegated_rc'] == 0)
    assert (rc_receiver['received_delegated_rc'] == 0)
    assert (rc_delegator['delegated_rc'] == 0)
    assert (rc_delegator['received_delegated_rc'] == 0)

    print('Delegating rc to {}'.format(receiver))
    wallet.api.delegate_rc(delegator, receiver, 10, 'true')

    rc_receiver = wallet.api.find_rc_accounts([receiver])['result'][0]
    rc_delegator = wallet.api.find_rc_accounts([delegator])['result'][0]

    assert (rc_receiver['delegated_rc'] == 0)
    assert (rc_receiver['received_delegated_rc'] == 10)
    assert (rc_delegator['delegated_rc'] == 10)
    assert (rc_delegator['received_delegated_rc'] == 0)
    assert (rc_delegator['rc_manabar']['current_mana'] == rc_delegator_before['rc_manabar']['current_mana'] - 13)  # 10 + 3, 3 is the base cost of an rc delegation op, 10 is the amount delegated

    print('testing list direct delegations api')

    delegation_from_to = wallet.api.list_rc_direct_delegations([delegator, receiver], 1000, 'by_from_to_sort')['result'][0]

    assert (delegation_from_to['from'] == delegator)
    assert (delegation_from_to['to'] == receiver)
    assert (delegation_from_to['delegated_rc'] == 10)

    print('Increasing the delegation to 50 to {}'.format(receiver))
    wallet.api.delegate_rc(delegator, receiver, 50, 'true')

    rc_receiver = wallet.api.find_rc_accounts([receiver])['result'][0]
    rc_delegator = wallet.api.find_rc_accounts([delegator])['result'][0]

    assert (rc_receiver['delegated_rc'] == 0)
    assert (rc_receiver['received_delegated_rc'] == 50)
    assert (rc_delegator['delegated_rc'] == 50)
    assert (rc_delegator['received_delegated_rc'] == 0)
    assert (rc_delegator['rc_manabar']['current_mana'] == rc_delegator_before['rc_manabar']['current_mana'] - 56)  # 50 + 3 x 2, 3 is the base cost of an rc delegation op, 50 is the amount delegated

    print('Reducing the delegation to 20 to {}'.format(receiver))
    wallet.api.delegate_rc(delegator, receiver, 20, 'true')

    rc_receiver = wallet.api.find_rc_accounts([receiver])['result'][0]
    rc_delegator = wallet.api.find_rc_accounts([delegator])['result'][0]

    assert (rc_receiver['delegated_rc'] == 0)
    assert (rc_receiver['received_delegated_rc'] == 20)
    assert (rc_delegator['delegated_rc'] == 20)
    assert (rc_delegator['received_delegated_rc'] == 0)
    assert (rc_delegator['rc_manabar']['current_mana'] == rc_delegator_before['rc_manabar'][
        'current_mana'] - 59)  # amount remains the same - 3 because current rc is not given back from reducing the delegation, and we paid 3 for the extra op

    print('deleting the delegation to {}'.format(receiver))
    wallet.api.delegate_rc(delegator, receiver, 0, 'true')

    rc_receiver = wallet.api.find_rc_accounts([receiver])['result'][0]
    rc_delegator = wallet.api.find_rc_accounts([delegator])['result'][0]
    delegation = wallet.api.list_rc_direct_delegations([delegator, receiver], 1000, 'by_from_to_sort')['result']

    assert (rc_receiver['delegated_rc'] == 0)
    assert (len(delegation) == 0)
    assert (rc_receiver['received_delegated_rc'] == 0)
    assert (rc_delegator['delegated_rc'] == 0)
    assert (rc_delegator['received_delegated_rc'] == 0)
    assert (rc_delegator['max_rc'] == rc_delegator_before['max_rc'])

    print("testing list_rc_accounts")
    accounts = wallet.api.list_rc_accounts('delegator', 3, 'by_name')['result']
    rc_delegator = wallet.api.find_rc_accounts([delegator])['result'][0]
    assert (len(accounts) == 3)
    assert (accounts[0]['account'] == 'delegator')
    assert (accounts[0]['rc_manabar']['current_mana'] == rc_delegator['rc_manabar']['current_mana'])
    assert (accounts[0]['rc_manabar']['last_update_time'] == rc_delegator['rc_manabar']['last_update_time'])
    assert (accounts[0]['max_rc'] == rc_delegator['max_rc'])
    assert (accounts[0]['delegated_rc'] == 0)
    assert (accounts[0]['received_delegated_rc'] == 0)
    assert (accounts[1]['account'] == 'hive.fund')

    accounts_all = wallet.api.list_rc_accounts('aaa', 100, 'by_name')
    assert (len(accounts_all) == 8)
    assert (accounts_all[7]['account'] == 'temp')

    accounts_offset = wallet.api.list_rc_accounts('receiver', 3, 'by_name')
    assert (len(accounts_offset) == 3)
    assert (accounts_offset[0]['account'] == 'receiver')
    assert (accounts_offset[0]['rc_manabar']['current_mana'] == 0)
    assert (accounts_offset[1]['account'] == 'steem.dao')
