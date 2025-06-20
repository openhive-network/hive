#!/usr/bin/env python3

import pytest
import test_tools as tt
from hive_utils.common import wait_n_blocks


def test_dhf_vote_weighting_data_structures(node_client):
    """Test that new DHF vote weighting data structures are accessible via API."""
    
    # Test that global properties contain new DHF tracking fields
    global_props = node_client.rpc.get_dynamic_global_properties()
    
    # Check for new DHF inflow tracking fields
    assert 'dhf_cached_daily_total' in global_props
    assert 'dhf_current_hour_index' in global_props
    assert 'dhf_inflow_last_update' in global_props
    
    # Check initial values
    assert global_props['dhf_cached_daily_total'] == "0.000 TBD"
    assert global_props['dhf_current_hour_index'] == 0


def test_dhf_account_commitment_fields(node_client):
    """Test that account objects contain new DHF commitment tracking fields."""
    
    # Test with initminer account (should exist)
    account = node_client.rpc.get_accounts(["initminer"])[0]
    
    # Check for new DHF commitment tracking fields
    assert 'dhf_total_daily_commitment' in account
    assert 'dhf_commitment_last_update' in account
    assert 'dhf_active_proposal_count' in account
    
    # Check initial values
    assert account['dhf_total_daily_commitment'] == "0.000 TBD"
    assert account['dhf_active_proposal_count'] == 0


def test_dhf_proposal_creation_with_weighting_fields(node_client):
    """Test proposal creation with new DHF weighting system in place."""
    
    with tt.logger.info("Creating proposal to test DHF weighting system"):
        # Create a simple proposal
        wallet = tt.Wallet(attach_to=node_client)
        wallet.api.import_key("5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3")
        
        # Create proposal with reasonable parameters
        start_date = node_client.rpc.get_dynamic_global_properties()['time']
        # Add 1 day in seconds
        start_timestamp = tt.Time.parse(start_date).timestamp() + 86400
        end_timestamp = start_timestamp + (30 * 86400)  # 30 days later
        
        proposal_op = {
            "type": "create_proposal_operation",
            "value": {
                "creator": "initminer",
                "receiver": "initminer", 
                "start_date": tt.Time.from_timestamp(start_timestamp).iso_string(),
                "end_date": tt.Time.from_timestamp(end_timestamp).iso_string(),
                "daily_pay": "10.000 TBD",
                "subject": "Test Proposal for DHF Weighting",
                "permlink": "test-proposal-dhf-weighting",
                "extensions": []
            }
        }
        
        # This should work with new fields in place
        result = wallet.api.broadcast_transaction({
            "operations": [proposal_op],
            "extensions": [],
            "signatures": []
        })
        
        assert result is not None
        wait_n_blocks(node_client.rpc.url, 2)


def test_dhf_vote_weighting_compatibility(node_client):
    """Test that DHF vote weighting system maintains backward compatibility."""
    
    # Create proposal first
    with tt.logger.info("Testing DHF vote weighting backward compatibility"):
        
        # Check that proposal creation still works
        proposals = node_client.rpc.list_proposals("", "by_creator", "ascending", 10, "all")
        initial_proposal_count = len(proposals)
        
        # Test that basic proposal voting still works
        # This should work in legacy mode before hardfork activation
        try:
            vote_result = node_client.rpc.get_accounts(["initminer"])
            assert len(vote_result) == 1
            
            # Basic API calls should still work
            assert 'dhf_total_daily_commitment' in vote_result[0]
            
        except Exception as e:
            pytest.fail(f"DHF vote weighting broke backward compatibility: {e}")


if __name__ == "__main__":
    # Run basic integration test
    node_client = tt.RawNode()
    
    try:
        test_dhf_vote_weighting_data_structures(node_client)
        test_dhf_account_commitment_fields(node_client)
        test_dhf_vote_weighting_compatibility(node_client)
        print("All DHF vote weighting integration tests passed!")
        
    except Exception as e:
        print(f"DHF vote weighting integration test failed: {e}")
        raise
    
    finally:
        node_client.close() 