#include <chainbase/forward_declarations.hpp>

#include <hive/protocol/sps_operations.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/hive_evaluator.hpp>
#include <hive/chain/sps_objects.hpp>

#include <hive/chain/util/sps_helper.hpp>


namespace hive { namespace chain {

using hive::chain::create_proposal_evaluator;

void create_proposal_evaluator::do_apply( const create_proposal_operation& o )
{
  try
  {
    FC_ASSERT( _db.has_hardfork( HIVE_PROPOSALS_HARDFORK ), "Proposals functionality not enabled until hardfork ${hf}", ("hf", HIVE_PROPOSALS_HARDFORK) );

    /** start date can be earlier than head_block_time - otherwise creating a proposal can be difficult,
        since passed date should be adjusted by potential transaction execution delay (i.e. 3 sec
        as a time for processed next block).
    */
    FC_ASSERT(o.end_date > _db.head_block_time(), "Can't create inactive proposals...");

    asset fee_hbd( HIVE_TREASURY_FEE, HBD_SYMBOL );

    if(_db.has_hardfork(HIVE_HARDFORK_1_24))
    {
      uint32_t proposal_run_time = o.end_date.sec_since_epoch() - o.start_date.sec_since_epoch();

      if(proposal_run_time > HIVE_PROPOSAL_FEE_INCREASE_DAYS_SEC)
      {
        uint32_t extra_days = (proposal_run_time / HIVE_ONE_DAY_SECONDS) - HIVE_PROPOSAL_FEE_INCREASE_DAYS;
        fee_hbd += asset(HIVE_PROPOSAL_FEE_INCREASE_AMOUNT * extra_days, HBD_SYMBOL);
      }
    }

    //treasury account must exist, also we need it later to change its balance
    const auto& treasury_account =_db.get_treasury();

    const auto& owner_account = _db.get_account( o.creator );
    const auto* receiver_account = _db.find_account( o.receiver );

    /// Just to check the receiver account exists.
    FC_ASSERT(receiver_account != nullptr, "Specified receiver account: ${r} must exist in the blockchain",
      ("r", o.receiver));

    const auto* commentObject = _db.find_comment(o.creator, o.permlink);
    if(commentObject == nullptr)
    {
      commentObject = _db.find_comment(o.receiver, o.permlink);
      FC_ASSERT(commentObject != nullptr, "Proposal permlink must point to the article posted by creator or receiver");
    }

    _db.create< proposal_object >( [&]( proposal_object& proposal )
    {
      proposal.proposal_id = proposal.get_id();

      proposal.creator = o.creator;
      proposal.receiver = o.receiver;

      proposal.start_date = o.start_date;
      proposal.end_date = o.end_date;

      proposal.daily_pay = o.daily_pay;

      proposal.subject = o.subject.c_str();

      proposal.permlink = o.permlink.c_str();
    });

    _db.adjust_balance( owner_account, -fee_hbd );
    /// Fee shall be paid to the treasury
    _db.adjust_balance(treasury_account, fee_hbd );
  }
  FC_CAPTURE_AND_RETHROW( (o) )
}

void update_proposal_evaluator::do_apply( const update_proposal_operation& o )
{
  try
  {
    FC_ASSERT( _db.has_hardfork( HIVE_HARDFORK_1_24 ), "The update proposal functionality not enabled until hardfork ${hf}", ("hf", HIVE_HARDFORK_1_24) );

    const auto& proposal = _db.get< proposal_object, by_proposal_id >( o.proposal_id );

    FC_ASSERT(o.creator == proposal.creator, "Cannot edit a proposal you are not the creator of");

    const auto* commentObject = _db.find_comment(proposal.creator, o.permlink);
    if(commentObject == nullptr)
    {
      commentObject = _db.find_comment(proposal.receiver, o.permlink);
      FC_ASSERT(commentObject != nullptr, "Proposal permlink must point to the article posted by creator or the receiver");
    }

    FC_ASSERT(o.daily_pay <= proposal.daily_pay, "You cannot increase the daily pay");

    const update_proposal_end_date* ed = nullptr;
    if (_db.has_hardfork(HIVE_HARDFORK_1_25)) {
      FC_ASSERT( o.extensions.size() < 2, "Cannot have more than 1 extension");
      // NOTE: This assumes there is only one extension and it's of type proposal_end_date, if you add more, update this code
      if (o.extensions.size() == 1) {
        ed = &(o.extensions.begin()->get<update_proposal_end_date>());
        FC_ASSERT(ed->end_date <= proposal.end_date, "You cannot increase the end date of the proposal");
        FC_ASSERT(ed->end_date > proposal.start_date, "The new end date must be after the start date");
      }
    } else {
      FC_ASSERT( o.extensions.empty() , "Cannot set extensions");
    }

    _db.modify( proposal, [&]( proposal_object& p )
    {
      p.daily_pay = o.daily_pay;
      p.subject = o.subject.c_str();
      p.permlink = o.permlink.c_str();

      if (_db.has_hardfork(HIVE_HARDFORK_1_25) && ed != nullptr) {
          p.end_date = ed->end_date;
      }
    });
  }
  FC_CAPTURE_AND_RETHROW( (o) )
}

void update_proposal_votes_evaluator::do_apply( const update_proposal_votes_operation& o )
{
  try
  {
    FC_ASSERT( _db.has_hardfork( HIVE_PROPOSALS_HARDFORK ), "Proposals functionality not enabled until hardfork ${hf}", ("hf", HIVE_PROPOSALS_HARDFORK) );

    const auto& pidx = _db.get_index< proposal_index >().indices().get< by_proposal_id >();
    const auto& pvidx = _db.get_index< proposal_vote_index >().indices().get< by_voter_proposal >();

    const auto& voter = _db.get_account(o.voter);
    _db.modify( voter, [&](account_object& a) { a.update_governance_vote_expiration_ts(_db.head_block_time()); });

    for( const auto pid : o.proposal_ids )
    {
      //checking if proposal id exists
      auto found_id = pidx.find( pid );
      if( found_id == pidx.end() || found_id->removed )
        continue;

      if( _db.has_hardfork( HIVE_HARDFORK_1_25 ) )
      {
        /*
          In the future is possible a situation, when it will be thousands proposals and some account will vote on each proposal.
          During the account's deactivation, all votes have to be removed immediately, so it's a risk of potential performance issue.
          Better it not to allow vote on expired proposal.
        */
        FC_ASSERT(_db.head_block_time() <= found_id->end_date, "Voting on expired proposals is not allowed...");
      }

      auto found = pvidx.find( boost::make_tuple( o.voter, pid ) );

      if( o.approve )
      {
        if( found == pvidx.end() )
          _db.create< proposal_vote_object >( [&]( proposal_vote_object& proposal_vote )
          {
            proposal_vote.voter = o.voter;
            proposal_vote.proposal_id = pid;
          } );
      }
      else
      {
        if( found != pvidx.end() )
          _db.remove( *found );
      }
    }
  }
  FC_CAPTURE_AND_RETHROW( (o) )
}

void remove_proposal_evaluator::do_apply(const remove_proposal_operation& op)
{
  try
  {
    FC_ASSERT( _db.has_hardfork( HIVE_PROPOSALS_HARDFORK ), "Proposals functionality not enabled until hardfork ${hf}", ("hf", HIVE_PROPOSALS_HARDFORK) );

    // Remove proposals and related votes...
    sps_helper::remove_proposals( _db, op.proposal_ids, op.proposal_owner );

    /*
      ...For performance reasons and the fact that proposal votes can accumulate over time but need to be removed along with proposals,
      process of proposal removal is subject to `common_remove_threshold`. Proposals and votes are physically removed above, however if
      some remain due to threshold being reached, the rest is marked with `removed` flag, to be actually removed during regular per-block cycles.
      The `end_date` is moved to `head_time - HIVE_PROPOSAL_MAINTENANCE_CLEANUP` so the proposals are ready to be removed immediately
      (see proposal_object::get_end_date_with_delay() - there was a short window when proposal was expired but still "alive" to avoid corner cases)
      flag `removed` - it's information for 'sps_api' plugin
      moving `end_date` - triggers the algorithm in `sps_processor::remove_proposals`

      When automatic actions will be introduced, this code will disappear.
    */
    auto new_end_date = _db.head_block_time() - fc::seconds( HIVE_PROPOSAL_MAINTENANCE_CLEANUP );
    for( const auto pid : op.proposal_ids )
    {
      const auto& pidx = _db.get_index< proposal_index, by_proposal_id >();

      auto found_id = pidx.find( pid );
      if( found_id == pidx.end() || found_id->removed )
        continue;
      
      _db.modify( *found_id, [new_end_date]( proposal_object& proposal )
      {
        proposal.removed = true;
        proposal.end_date = new_end_date;
      } );
    }

  }
  FC_CAPTURE_AND_RETHROW( (op) )
}

} } // hive::chain
