#include <boost/exception/diagnostic_information.hpp>
#include <boost/program_options.hpp>

#include <fc/filesystem.hpp>
#include <fc/exception/exception.hpp>

#include <hive/chain/block_log.hpp>

#include <iostream>
#include <map>
#include <vector>
#include <string>

using namespace hive::chain;
namespace bpo = boost::program_options;

int main( int argc, char** argv )
{
  try
  {
    // Setup operation counter options
    bpo::options_description opts;
      opts.add_options()
      ("help,h", "Print this help message and exit.")
      ("input,i", bpo::value< std::string >(), "input block log")
      ("operations,o", bpo::value< std::vector< std::string > >()->multitoken()->default_value( { "all" }, "all"), "operations to count (e.g. pow_operation,pow2_operation) or all to count all of them")
      ("offset,s", bpo::value< uint32_t >()->default_value( 0 ), "offset for the block_log to start iteration" )
      ("blocks-limit,l", bpo::value< uint32_t >()->default_value( 0 ), "stop counting operations after given limit of blocks. 0 for no limit")
      ;

    bpo::variables_map options;

    bpo::store( bpo::parse_command_line(argc, argv, opts), options );

    if( options.count("help") || !options.count("input") )
    {
      std::cout << "Counts the number of the given operation types in the block_log\n"
        << opts << "\n";
      return 0;
    }

    std::map< /* op name: */std::string, /* num of ops: */uint32_t > counter;

    block_log log_in;
    log_in.open( fc::path(options["input"].as< std::string >()) );

    uint32_t block_num = 1 + options["offset"].as< uint32_t >();
    uint32_t blocks_limit = options["blocks-limit"].as< uint32_t >() ? block_num+options["blocks-limit"].as< uint32_t >() - 1 : log_in.head()->block_num();
    if( blocks_limit > log_in.head()->block_num() )
      blocks_limit = log_in.head()->block_num();

    std::cout << "Reading blocks: " << block_num << " to " << blocks_limit << '\n';

#ifdef HIVE_ENABLE_SMT
    const uint64_t smt_ops_continue = 56;
#else
    const uint64_t smt_ops_continue = 49;
#endif

    for( ; block_num <= blocks_limit; ++block_num )
    {
      fc::optional< signed_block > block = log_in.read_block_by_num( block_num );
      FC_ASSERT( block.valid(), "unable to read block" );

      for( auto _transaction = (*block).transactions.begin(); _transaction != (*block).transactions.end(); ++_transaction )
        for( auto op = _transaction->operations.begin(); op != _transaction->operations.end(); ++op )
          switch( op->which() )
          {
            case  0: ++counter[ "vote_operation" ]; break;
            case  1: ++counter[ "comment_operation" ]; break;
            case  2: ++counter[ "transfer_operation" ]; break;
            case  3: ++counter[ "transfer_to_vesting_operation" ]; break;
            case  4: ++counter[ "withdraw_vesting_operation" ]; break;
            case  5: ++counter[ "limit_order_create_operation" ]; break;
            case  6: ++counter[ "limit_order_cancel_operation" ]; break;
            case  7: ++counter[ "feed_publish_operation" ]; break;
            case  8: ++counter[ "convert_operation" ]; break;
            case  9: ++counter[ "account_create_operation" ]; break;
            case 10: ++counter[ "account_update_operation" ]; break;
            case 11: ++counter[ "witness_update_operation" ]; break;
            case 12: ++counter[ "account_witness_vote_operation" ]; break;
            case 13: ++counter[ "account_witness_proxy_operation" ]; break;
            case 14: ++counter[ "pow_operation" ]; break;
            case 15: ++counter[ "custom_operation" ]; break;
            case 16: ++counter[ "report_over_production_operation" ]; break;
            case 17: ++counter[ "delete_comment_operation" ]; break;
            case 18: ++counter[ "custom_json_operation" ]; break;
            case 19: ++counter[ "comment_options_operation" ]; break;
            case 20: ++counter[ "set_withdraw_vesting_route_operation" ]; break;
            case 21: ++counter[ "limit_order_create2_operation" ]; break;
            case 22: ++counter[ "claim_account_operation" ]; break;
            case 23: ++counter[ "create_claimed_account_operation" ]; break;
            case 24: ++counter[ "request_account_recovery_operation" ]; break;
            case 25: ++counter[ "recover_account_operation" ]; break;
            case 26: ++counter[ "change_recovery_account_operation" ]; break;
            case 27: ++counter[ "escrow_transfer_operation" ]; break;
            case 28: ++counter[ "escrow_dispute_operation" ]; break;
            case 29: ++counter[ "escrow_release_operation" ]; break;
            case 30: ++counter[ "pow2_operation" ]; break;
            case 31: ++counter[ "escrow_approve_operation" ]; break;
            case 32: ++counter[ "transfer_to_savings_operation" ]; break;
            case 33: ++counter[ "transfer_from_savings_operation" ]; break;
            case 34: ++counter[ "cancel_transfer_from_savings_operation" ]; break;
            case 35: ++counter[ "custom_binary_operation" ]; break;
            case 36: ++counter[ "decline_voting_rights_operation" ]; break;
            case 37: ++counter[ "reset_account_operation" ]; break;
            case 38: ++counter[ "set_reset_account_operation" ]; break;
            case 39: ++counter[ "claim_reward_balance_operation" ]; break;
            case 40: ++counter[ "delegate_vesting_shares_operation" ]; break;
            case 41: ++counter[ "account_create_with_delegation_operation" ]; break;
            case 42: ++counter[ "witness_set_properties_operation" ]; break;
            case 43: ++counter[ "account_update2_operation" ]; break;
            case 44: ++counter[ "create_proposal_operation" ]; break;
            case 45: ++counter[ "update_proposal_votes_operation" ]; break;
            case 46: ++counter[ "remove_proposal_operation" ]; break;
            case 47: ++counter[ "update_proposal_operation" ]; break;
            case 48: ++counter[ "collateralized_convert_operation" ]; break;
#ifdef HIVE_ENABLE_SMT
            case 49: ++counter[ "claim_reward_balance2_operation" ]; break;
            case 50: ++counter[ "smt_setup_operation" ]; break;
            case 51: ++counter[ "smt_setup_emissions_operation" ]; break;
            case 52: ++counter[ "smt_set_setup_parameters_operation" ]; break;
            case 53: ++counter[ "smt_set_runtime_parameters_operation" ]; break;
            case 54: ++counter[ "smt_create_operation" ]; break;
            case 55: ++counter[ "smt_contribute_operation" ]; break;
#endif
            case smt_ops_continue+ 0: ++counter[ "fill_convert_request_operation" ]; break;
            case smt_ops_continue+ 1: ++counter[ "author_reward_operation" ]; break;
            case smt_ops_continue+ 2: ++counter[ "curation_reward_operation" ]; break;
            case smt_ops_continue+ 3: ++counter[ "comment_reward_operation" ]; break;
            case smt_ops_continue+ 4: ++counter[ "liquidity_reward_operation" ]; break;
            case smt_ops_continue+ 5: ++counter[ "interest_operation" ]; break;
            case smt_ops_continue+ 6: ++counter[ "fill_vesting_withdraw_operation" ]; break;
            case smt_ops_continue+ 7: ++counter[ "fill_order_operation" ]; break;
            case smt_ops_continue+ 8: ++counter[ "shutdown_witness_operation" ]; break;
            case smt_ops_continue+ 9: ++counter[ "fill_transfer_from_savings_operation" ]; break;
            case smt_ops_continue+10: ++counter[ "hardfork_operation" ]; break;
            case smt_ops_continue+11: ++counter[ "comment_payout_update_operation" ]; break;
            case smt_ops_continue+12: ++counter[ "return_vesting_delegation_operation" ]; break;
            case smt_ops_continue+13: ++counter[ "comment_benefactor_reward_operation" ]; break;
            case smt_ops_continue+14: ++counter[ "producer_reward_operation" ]; break;
            case smt_ops_continue+15: ++counter[ "clear_null_account_balance_operation" ]; break;
            case smt_ops_continue+16: ++counter[ "proposal_pay_operation" ]; break;
            case smt_ops_continue+17: ++counter[ "sps_fund_operation" ]; break;
            case smt_ops_continue+18: ++counter[ "hardfork_hive_operation" ]; break;
            case smt_ops_continue+19: ++counter[ "hardfork_hive_restore_operation" ]; break;
            case smt_ops_continue+20: ++counter[ "delayed_voting_operation" ]; break;
            case smt_ops_continue+21: ++counter[ "consolidate_treasury_balance_operation" ]; break;
            case smt_ops_continue+22: ++counter[ "effective_comment_vote_operation" ]; break;
            case smt_ops_continue+23: ++counter[ "ineffective_delete_comment_operation" ]; break;
            case smt_ops_continue+24: ++counter[ "sps_convert_operation" ]; break;
            case smt_ops_continue+25: ++counter[ "expired_account_notification_operation" ]; break;
            case smt_ops_continue+26: ++counter[ "changed_recovery_account_operation" ]; break;
            case smt_ops_continue+27: ++counter[ "transfer_to_vesting_completed_operation" ]; break;
            case smt_ops_continue+28: ++counter[ "pow_reward_operation" ]; break;
            case smt_ops_continue+29: ++counter[ "vesting_shares_split_operation" ]; break;
            case smt_ops_continue+30: ++counter[ "account_created_operation" ]; break;
            case smt_ops_continue+31: ++counter[ "fill_collateralized_convert_request_operation" ]; break;
            case smt_ops_continue+32: ++counter[ "system_warning_operation" ]; break;
            default: ++counter[ "unknown" ];
          }

      if( block_num % 1000 == 0 ) // Progress
      {
        std::cout << "[ " << int( float(block_num) / blocks_limit * 100 ) << "% ]: " << block_num << '/' << blocks_limit << " blocks parsed.\r";
        std::cout.flush();
      }
    }

    log_in.close();

    std::cout << '\n';

    bool display_all_ops = false;
    for( auto& opn : options["operations"].as< std::vector< std::string > >() )
      if( opn == "all" )
      {
        display_all_ops = true;
        break;
      }

    if( display_all_ops )
      for( auto& opt : counter )
          std::cout << opt.first << ": " << opt.second << '\n';
    else
    {
      for( auto& opt : options["operations"].as< std::vector< std::string > >() )
      {
        auto op_itr = counter.find( opt );
        if( op_itr != counter.end() )
          std::cout << op_itr->first << ": " << op_itr->second << '\n';
        else
          std::cout << opt << ": 0\n";
      }
    }

    return 0;
  }
  catch ( const boost::exception& e )
  {
    std::cerr << boost::diagnostic_information(e) << "\n";
  }
  catch ( const fc::exception& e )
  {
    std::cerr << e.to_detail_string() << "\n";
  }
  catch ( const std::exception& e )
  {
    std::cerr << e.what() << "\n";
  }
  catch ( ... )
  {
    std::cerr << "unknown exception\n";
  }

  return -1;
}
