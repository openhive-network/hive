import pytest

from test_tools import exceptions


@pytest.mark.parametrize(
    'wallet_bridge_api_command', [
        'get_version',
        'get_chain_properties',
        'get_witness_schedule',
        'get_current_median_history_price',
        'get_hardfork_version',
        'get_feed_history',
        'get_active_witnesses',
        'get_withdraw_routes',
        'get_dynamic_global_properties',
        'get_accounts',
        'list_witnesses',
        'get_witness',
        'get_conversion_requests',
        'get_collateralized_conversion_requests',
        'get_owner_history',
        'list_proposals',
        'find_proposals',
        'list_proposal_votes',
    ]
)
@pytest.mark.enabled_plugins('witness', 'wallet_bridge_api')
def test_reporting_exception_when_database_api_is_missing(node, wallet_bridge_api_command):
    with pytest.raises(exceptions.CommunicationError) as exception:
        getattr(node.api.wallet_bridge, wallet_bridge_api_command)()

    assert 'Assert Exception:_database_api: database_api_plugin not enabled.' in str(exception.value)


@pytest.mark.parametrize(
    'wallet_bridge_api_command', [
        'list_rc_accounts',
        'list_rc_direct_delegations',
    ]
)
@pytest.mark.enabled_plugins('witness', 'wallet_bridge_api')
def test_reporting_exception_when_rc_api_is_missing(node, wallet_bridge_api_command):
    with pytest.raises(exceptions.CommunicationError) as exception:
        getattr(node.api.wallet_bridge, wallet_bridge_api_command)()

    assert 'Assert Exception:_rc_api: rc_api_plugin not enabled.' in str(exception.value)


@pytest.mark.parametrize(
    'wallet_bridge_api_command', [
        'get_block'
    ]
)
@pytest.mark.enabled_plugins('witness', 'wallet_bridge_api')
def test_reporting_exception_when_block_api_is_missing(node, wallet_bridge_api_command):
    with pytest.raises(exceptions.CommunicationError) as exception:
        getattr(node.api.wallet_bridge, wallet_bridge_api_command)()

    assert 'Assert Exception:_block_api: block_api_plugin not enabled.' in str(exception.value)


@pytest.mark.parametrize(
    'wallet_bridge_api_command', [
        'get_ops_in_block',
        'get_transaction',
        'get_account_history',
    ]
)
@pytest.mark.enabled_plugins('witness', 'wallet_bridge_api')
def test_reporting_exception_when_account_history_api_is_missing(node, wallet_bridge_api_command):
    with pytest.raises(exceptions.CommunicationError) as exception:
        getattr(node.api.wallet_bridge, wallet_bridge_api_command)()

    assert 'Assert Exception:_account_history_api: account_history_api_plugin not enabled.' in str(exception.value)


@pytest.mark.parametrize(
    'wallet_bridge_api_command', [
        'list_my_accounts',
    ]
)
@pytest.mark.enabled_plugins('witness', 'wallet_bridge_api')
def test_reporting_exception_when_account_by_key_api_is_missing(node, wallet_bridge_api_command):
    with pytest.raises(exceptions.CommunicationError) as exception:
        getattr(node.api.wallet_bridge, wallet_bridge_api_command)()

    assert 'Assert Exception:_account_by_key_api: account_by_key_api_plugin not enabled.' in str(exception.value)


@pytest.mark.parametrize(
    'wallet_bridge_api_command', [
        'get_order_book'
    ]
)
@pytest.mark.enabled_plugins('witness', 'wallet_bridge_api')
def test_reporting_exception_when_market_history_api_is_missing(node, wallet_bridge_api_command):
    with pytest.raises(exceptions.CommunicationError) as exception:
        getattr(node.api.wallet_bridge, wallet_bridge_api_command)()

    assert 'Assert Exception:_market_history_api: market_history_api_plugin not enabled.' in str(exception.value)


@pytest.mark.enabled_plugins('witness')
def test_reporting_exception_when_wallet_bridge_api_is_missing(node):
    with pytest.raises(exceptions.CommunicationError) as exception:
        getattr(node.api.wallet_bridge, 'get_version')()

    assert 'Could not find API wallet_bridge_api' in str(exception.value)
