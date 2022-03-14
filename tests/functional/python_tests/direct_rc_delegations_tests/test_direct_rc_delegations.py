import test_tools as tt


def test_direct_rc_delegations(wallet):
    creator = 'initminer'
    delegator = 'delegator'
    receiver = 'receiver'
    receiver2 = 'zzz'
    wallet.api.create_account(creator, delegator, '{}')
    wallet.api.create_account(creator, receiver, '{}')
    wallet.api.create_account(creator, receiver2, '{}')
    wallet.api.transfer(creator, receiver, '10.000 TESTS', '', 'true')
    wallet.api.transfer_to_vesting(creator, delegator, '0.010 TESTS', 'true')
    try:
        wallet.api.transfer(receiver, receiver, '0.001 TESTS', '', 'true')
    except tt.exceptions.CommunicationError as e:
        message = str(e.response)
        found = message.find('receiver has 0 RC, needs 4 RC. Please wait to transact')
        assert found != -1
    rc_receiver = wallet.api.find_rc_accounts([receiver])[0]
    rc_receiver2 = wallet.api.find_rc_accounts([receiver2])[0]
    rc_delegator = wallet.api.find_rc_accounts([delegator])[0]
    rc_delegator_before = rc_delegator

    assert (rc_receiver['delegated_rc'] == 0)
    assert (rc_receiver['received_delegated_rc'] == 0)
    assert (rc_delegator['delegated_rc'] == 0)
    assert (rc_delegator['received_delegated_rc'] == 0)
    assert (rc_receiver2['delegated_rc'] == 0)
    assert (rc_receiver2['received_delegated_rc'] == 0)

    print('Delegating rc to {} and {}'.format(receiver, receiver2))
    wallet.api.delegate_rc(delegator, [receiver, receiver2], 10, 'true')

    rc_receiver = wallet.api.find_rc_accounts([receiver])[0]
    rc_receiver2 = wallet.api.find_rc_accounts([receiver2])[0]
    rc_delegator = wallet.api.find_rc_accounts([delegator])[0]

    assert (rc_receiver['delegated_rc'] == 0)
    assert (rc_receiver['received_delegated_rc'] == 10)
    assert (rc_receiver2['delegated_rc'] == 0)
    assert (rc_receiver2['received_delegated_rc'] == 10)
    assert (rc_delegator['delegated_rc'] == 20)
    assert (rc_delegator['received_delegated_rc'] == 0)
    assert (rc_delegator['rc_manabar']['current_mana'] == rc_delegator_before['rc_manabar'][
        'current_mana'] - 23)  # 20 + 3, 3 is the base cost of an rc delegation op, 20 is the amount delegated

    print('testing list direct delegations api')

    delegation_from_to = \
        wallet.api.list_rc_direct_delegations([delegator, receiver], 1000)[0]

    assert (delegation_from_to['from'] == delegator)
    assert (delegation_from_to['to'] == receiver)
    assert (delegation_from_to['delegated_rc'] == 10)

    print('Increasing the delegation to 50 to {}'.format(receiver))
    wallet.api.delegate_rc(delegator, [receiver], 50, 'true')

    rc_receiver = wallet.api.find_rc_accounts([receiver])[0]
    rc_delegator = wallet.api.find_rc_accounts([delegator])[0]

    assert (rc_receiver['delegated_rc'] == 0)
    assert (rc_receiver['received_delegated_rc'] == 50)
    assert (rc_delegator['delegated_rc'] == 60)
    assert (rc_delegator['received_delegated_rc'] == 0)
    assert (rc_delegator['rc_manabar']['current_mana'] == rc_delegator_before['rc_manabar'][
        'current_mana'] - 66)  # 50 + 3 x 2, 3 is the base cost of an rc delegation op, 50 is the amount delegated

    print('Reducing the delegation to 20 to {}'.format(receiver))
    wallet.api.delegate_rc(delegator, [receiver], 20, 'true')

    rc_receiver = wallet.api.find_rc_accounts([receiver])[0]
    rc_delegator = wallet.api.find_rc_accounts([delegator])[0]

    assert (rc_receiver['delegated_rc'] == 0)
    assert (rc_receiver['received_delegated_rc'] == 20)
    assert (rc_delegator['delegated_rc'] == 30)
    assert (rc_delegator['received_delegated_rc'] == 0)
    assert (rc_delegator['rc_manabar']['current_mana'] == rc_delegator_before['rc_manabar'][
        'current_mana'] - 69)  # amount remains the same - 3 because current rc is not given back from reducing the delegation, and we paid 3 for the extra op

    print('deleting the delegation to {}'.format(receiver))
    wallet.api.delegate_rc(delegator, [receiver], 0, 'true')

    rc_receiver = wallet.api.find_rc_accounts([receiver])[0]
    rc_delegator = wallet.api.find_rc_accounts([delegator])[0]
    delegation = wallet.api.list_rc_direct_delegations([delegator, receiver], 1000)

    assert (rc_receiver['delegated_rc'] == 0)
    assert (len(delegation) == 1)
    assert (rc_receiver['received_delegated_rc'] == 0)
    assert (rc_delegator['delegated_rc'] == 10)
    assert (rc_delegator['received_delegated_rc'] == 0)
    assert (rc_delegator['max_rc'] == rc_delegator_before['max_rc'] - 10)

    print("testing list_rc_accounts")
    accounts = wallet.api.list_rc_accounts('delegator', 3)
    rc_delegator = wallet.api.find_rc_accounts([delegator])[0]
    assert (len(accounts) == 3)
    assert (accounts[0]['account'] == 'delegator')
    assert (accounts[0]['rc_manabar']['current_mana'] == rc_delegator['rc_manabar']['current_mana'])
    assert (accounts[0]['rc_manabar']['last_update_time'] == rc_delegator['rc_manabar']['last_update_time'])
    assert (accounts[0]['max_rc'] == rc_delegator['max_rc'])
    assert (accounts[0]['delegated_rc'] == 10)
    assert (accounts[0]['received_delegated_rc'] == 0)
    assert (accounts[1]['account'] == 'hive.fund')

    accounts_all = wallet.api.list_rc_accounts('aaa', 100)
    assert (len(accounts_all) == 9)

    accounts_offset = wallet.api.list_rc_accounts('receiver', 3)
    assert (len(accounts_offset) == 3)
    assert (accounts_offset[0]['account'] == 'receiver')
    assert (accounts_offset[0]['rc_manabar']['current_mana'] == 0)
    assert (accounts_offset[1]['account'] == 'steem.dao')

