
#include <hive/chain/hive_fwd.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/witness_objects.hpp>
#include <hive/chain/witness_schedule.hpp>

#include <hive/chain/rc/rc_utility.hpp>
#include <hive/chain/util/rd_setup.hpp>

#include <hive/protocol/config.hpp>

namespace hive { namespace chain {

using hive::chain::util::rd_system_params;
using hive::chain::util::rd_user_params;
using hive::chain::util::rd_validate_user_params;
using hive::protocol::system_warning_operation;

void reset_virtual_schedule_time( database& db )
{ try {
  const witness_schedule_object& wso = db.has_hardfork(HIVE_HARDFORK_1_27_FIX_TIMESHARE_WITNESS_SCHEDULING) ?
                                       db.get_witness_schedule_object_for_irreversibility() : db.get_witness_schedule_object();
  db.modify( wso, [&](witness_schedule_object& o )
  {
    o.current_virtual_time = fc::uint128(); // reset it 0
  } );

  const auto& idx = db.get_index<witness_index>().indices();
  for( const auto& witness : idx )
  {
    db.modify( witness, [&]( witness_object& wobj )
    {
      wobj.virtual_position = fc::uint128();
      wobj.virtual_last_update = wso.current_virtual_time;
      wobj.virtual_scheduled_time = HIVE_VIRTUAL_SCHEDULE_LAP_LENGTH2 / (wobj.votes.value+1);
    } );
  }
} FC_CAPTURE_AND_RETHROW() }

void update_median_witness_props(database& db, const witness_schedule_object& wso)
{ try {
  /// fetch all witness objects
  vector<const witness_object*> active; active.reserve( wso.num_scheduled_witnesses );
  for( int i = 0; i < wso.num_scheduled_witnesses; i++ )
  {
    active.push_back( &db.get_witness( wso.current_shuffled_witnesses[i] ) );
  }

  /// sort them by account_creation_fee
  std::sort( active.begin(), active.end(), [&]( const witness_object* a, const witness_object* b )
  {
    return a->props.account_creation_fee.amount < b->props.account_creation_fee.amount;
  } );
  asset median_account_creation_fee = active[active.size()/2]->props.account_creation_fee;

  /// sort them by maximum_block_size
  std::sort( active.begin(), active.end(), [&]( const witness_object* a, const witness_object* b )
  {
    return a->props.maximum_block_size < b->props.maximum_block_size;
  } );
  uint32_t median_maximum_block_size = active[active.size()/2]->props.maximum_block_size;

  /// sort them by hbd_interest_rate
  std::sort( active.begin(), active.end(), [&]( const witness_object* a, const witness_object* b )
  {
    return a->props.hbd_interest_rate < b->props.hbd_interest_rate;
  } );
  uint16_t median_hbd_interest_rate = active[active.size()/2]->props.hbd_interest_rate;

  /// sort them by account_subsidy_budget
  std::sort( active.begin(), active.end(), [&]( const witness_object* a, const witness_object* b )
  {
    return a->props.account_subsidy_budget < b->props.account_subsidy_budget;
  } );
  int32_t median_account_subsidy_budget = active[active.size()/2]->props.account_subsidy_budget;

  /// sort them by account_subsidy_decay
  std::sort( active.begin(), active.end(), [&]( const witness_object* a, const witness_object* b )
  {
    return a->props.account_subsidy_decay < b->props.account_subsidy_decay;
  });
  uint32_t median_account_subsidy_decay = active[active.size()/2]->props.account_subsidy_decay;

  // sort them by pool level
  std::sort( active.begin(), active.end(), [&]( const witness_object* a, const witness_object* b )
  {
    return a->available_witness_account_subsidies < b->available_witness_account_subsidies;
  });
  int64_t median_available_witness_account_subsidies = active[active.size()/2]->available_witness_account_subsidies;

  rd_system_params account_subsidy_system_params;
  account_subsidy_system_params.resource_unit = HIVE_ACCOUNT_SUBSIDY_PRECISION;
  account_subsidy_system_params.decay_per_time_unit_denom_shift = HIVE_RD_DECAY_DENOM_SHIFT;
  rd_user_params account_subsidy_user_params;
  account_subsidy_user_params.budget_per_time_unit = median_account_subsidy_budget;
  account_subsidy_user_params.decay_per_time_unit = median_account_subsidy_decay;

  rd_user_params account_subsidy_per_witness_user_params;
  int64_t w_budget = median_account_subsidy_budget;
  w_budget = (w_budget * HIVE_WITNESS_SUBSIDY_BUDGET_PERCENT) / HIVE_100_PERCENT;
  w_budget = std::min( w_budget, int64_t(std::numeric_limits<int32_t>::max()) );
  uint64_t w_decay = median_account_subsidy_decay;
  w_decay = (w_decay * HIVE_WITNESS_SUBSIDY_DECAY_PERCENT) / HIVE_100_PERCENT;
  w_decay = std::min( w_decay, uint64_t(std::numeric_limits<uint32_t>::max()) );

  account_subsidy_per_witness_user_params.budget_per_time_unit = int32_t(w_budget);
  account_subsidy_per_witness_user_params.decay_per_time_unit = uint32_t(w_decay);

  // Should never fail, as validate_user_params() is checked and median of valid params should always be valid
  rd_validate_user_params( account_subsidy_user_params );

  db.modify( wso, [&]( witness_schedule_object& _wso )
  {
    _wso.median_props.account_creation_fee       = median_account_creation_fee;
    _wso.median_props.maximum_block_size         = median_maximum_block_size;
    _wso.median_props.hbd_interest_rate          = median_hbd_interest_rate;
    _wso.median_props.account_subsidy_budget     = median_account_subsidy_budget;
    _wso.median_props.account_subsidy_decay      = median_account_subsidy_decay;

    rd_setup_dynamics_params( account_subsidy_user_params, account_subsidy_system_params, _wso.account_subsidy_rd );
    rd_setup_dynamics_params( account_subsidy_per_witness_user_params, account_subsidy_system_params, _wso.account_subsidy_witness_rd );

    int64_t median_decay = rd_compute_pool_decay( _wso.account_subsidy_witness_rd.decay_params, median_available_witness_account_subsidies, 1 );
    median_decay = std::max( median_decay, int64_t(0) );
    int64_t min_decay = fc::uint128_to_int64(fc::uint128( median_decay ) * HIVE_DECAY_BACKSTOP_PERCENT / HIVE_100_PERCENT);
    _wso.account_subsidy_witness_rd.min_decay = min_decay;
  } );

  const dynamic_global_property_object& dgpo = db.get_dynamic_global_properties();
  if( dgpo.maximum_block_size != median_maximum_block_size )
  {
    db.push_virtual_operation( system_warning_operation( FC_LOG_MESSAGE( warn,
      "Changing maximum block size from ${old} to ${new}", ( "old", dgpo.maximum_block_size )( "new", median_maximum_block_size ) ).get_message() ) );
  }
  db.modify( dgpo, [&]( dynamic_global_property_object& _dgpo )
  {
    _dgpo.maximum_block_size = median_maximum_block_size;
    _dgpo.hbd_interest_rate  = median_hbd_interest_rate;
  } );
} FC_CAPTURE_AND_RETHROW() }

void update_witness_schedule4(database& db, const witness_schedule_object& wso)
{ try {
  vector< account_name_type > active_witnesses;
  active_witnesses.reserve( HIVE_MAX_WITNESSES );

  /// Add the highest voted witnesses
  flat_set< witness_id_type > selected_voted;
  selected_voted.reserve( wso.max_voted_witnesses );

  const auto& widx = db.get_index<witness_index>().indices().get<by_vote_name>();
  for( auto itr = widx.begin();
    itr != widx.end() && selected_voted.size() < wso.max_voted_witnesses;
    ++itr )
  {
    if( db.has_hardfork( HIVE_HARDFORK_0_14__278 ) && (itr->signing_key == public_key_type()) )
      continue;
    selected_voted.insert( itr->get_id() );
    active_witnesses.push_back( itr->owner) ;
    db.modify( *itr, [&]( witness_object& wo ) { wo.schedule = witness_object::elected; } );
  }

  auto num_elected = active_witnesses.size();

  /// Add miners from the top of the mining queue
  flat_set< witness_id_type > selected_miners;
  selected_miners.reserve( wso.max_miner_witnesses );
  const auto& gprops = db.get_dynamic_global_properties();
  const auto& pow_idx      = db.get_index<witness_index>().indices().get<by_pow>();
  auto mitr = pow_idx.upper_bound(0);
  while( mitr != pow_idx.end() && selected_miners.size() < wso.max_miner_witnesses )
  {
    // Only consider a miner who is not a top voted witness
    if( selected_voted.find( mitr->get_id() ) == selected_voted.end() )
    {
      // Only consider a miner who has a valid block signing key
      if( !( db.has_hardfork( HIVE_HARDFORK_0_14__278 ) && db.get_witness( mitr->owner ).signing_key == public_key_type() ) )
      {
        selected_miners.insert( mitr->get_id() );
        active_witnesses.push_back(mitr->owner);
        db.modify( *mitr, [&]( witness_object& wo ) { wo.schedule = witness_object::miner; } );
      }
    }
    // Remove processed miner from the queue
    auto itr = mitr;
    ++mitr;
    db.modify( *itr, [&](witness_object& wit )
    {
      wit.pow_worker = 0;
    } );
    db.modify( gprops, [&]( dynamic_global_property_object& obj )
    {
      obj.num_pow_witnesses--;
    } );
  }

  auto num_miners = selected_miners.size();

  /// Add the running witnesses in the lead
  fc::uint128 new_virtual_time = wso.current_virtual_time;
  const auto& schedule_idx = db.get_index<witness_index>().indices().get<by_schedule_time>();
  auto sitr = schedule_idx.begin();
  vector<decltype(sitr)> processed_witnesses;
  for( auto witness_count = selected_voted.size() + selected_miners.size();
    sitr != schedule_idx.end() && witness_count < HIVE_MAX_WITNESSES;
    ++sitr )
  {
    new_virtual_time = sitr->virtual_scheduled_time; /// everyone advances to at least this time
    processed_witnesses.push_back(sitr);

    if( db.has_hardfork( HIVE_HARDFORK_0_14__278 ) && sitr->signing_key == public_key_type() )
      continue; /// skip witnesses without a valid block signing key

    if( selected_miners.find( sitr->get_id() ) == selected_miners.end()
      && selected_voted.find( sitr->get_id() ) == selected_voted.end() )
    {
      active_witnesses.push_back(sitr->owner);
      db.modify( *sitr, [&]( witness_object& wo ) { wo.schedule = witness_object::timeshare; } );
      ++witness_count;
    }
  }

  auto num_timeshare = active_witnesses.size() - num_miners - num_elected;

  /// Update virtual schedule of processed witnesses
  bool reset_virtual_time = false;
  for( auto itr = processed_witnesses.begin(); itr != processed_witnesses.end(); ++itr )
  {
    auto new_virtual_scheduled_time = new_virtual_time + HIVE_VIRTUAL_SCHEDULE_LAP_LENGTH2 / ((*itr)->votes.value+1);
    if( new_virtual_scheduled_time < new_virtual_time )
    {
      reset_virtual_time = true; /// overflow
      break;
    }
    db.modify( *(*itr), [&]( witness_object& wo )
    {
      wo.virtual_position        = fc::uint128();
      wo.virtual_last_update     = new_virtual_time;
      wo.virtual_scheduled_time  = new_virtual_scheduled_time;
    } );
  }
  if( reset_virtual_time )
  {
    new_virtual_time = fc::uint128();
    reset_virtual_schedule_time(db);
  }

  size_t expected_active_witnesses = std::min( size_t(HIVE_MAX_WITNESSES), widx.size() );
  FC_ASSERT( active_witnesses.size() == expected_active_witnesses, "number of active witnesses does not equal expected_active_witnesses=${expected_active_witnesses}",
                          ("active_witnesses.size()",active_witnesses.size()) ("HIVE_MAX_WITNESSES",HIVE_MAX_WITNESSES) ("expected_active_witnesses", expected_active_witnesses) );

  auto majority_version = wso.majority_version;

  if( db.has_hardfork( HIVE_HARDFORK_0_5__54 ) )
  {
    flat_map< version, uint32_t, std::greater< version > > witness_versions;
    flat_map< std::tuple< hardfork_version, time_point_sec >, uint32_t > hardfork_version_votes;

    for( uint32_t i = 0; i < wso.num_scheduled_witnesses; i++ )
    {
      auto& witness = db.get_witness( wso.current_shuffled_witnesses[ i ] );
      if( witness_versions.find( witness.running_version ) == witness_versions.end() )
        witness_versions[ witness.running_version ] = 1;
      else
        witness_versions[ witness.running_version ] += 1;

      auto version_vote = std::make_tuple( witness.hardfork_version_vote, witness.hardfork_time_vote );
      if( hardfork_version_votes.find( version_vote ) == hardfork_version_votes.end() )
        hardfork_version_votes[ version_vote ] = 1;
      else
        hardfork_version_votes[ version_vote ] += 1;
    }

    int witnesses_on_version = 0;
    auto ver_itr = witness_versions.begin();

    const auto& hpo = db.get_hardfork_property_object();

    if( hpo.current_hardfork_version == HIVE_HARDFORK_0_22_VERSION )
    {
      if( hpo.next_hardfork != HIVE_HARDFORK_0_23_VERSION )
      {
        ilog("Forcing HF23 without need to have witness majority version");

        /// Force HF23 even no witness majority.
        db.modify(hpo, [&](hardfork_property_object& _hpo)
        {
          _hpo.next_hardfork = HIVE_HARDFORK_0_23_VERSION;
          _hpo.next_hardfork_time = fc::time_point_sec(HIVE_HARDFORK_0_23_TIME);
        });
      }
    }
    else
    {
      // The map should be sorted highest version to smallest, so we iterate until we hit the majority of witnesses on at least this version
      while( ver_itr != witness_versions.end() )
      {
        witnesses_on_version += ver_itr->second;

        if( witnesses_on_version >= wso.hardfork_required_witnesses )
        {
          majority_version = ver_itr->first;
          break;
        }

        ++ver_itr;
      }

      auto hf_itr = hardfork_version_votes.begin();

      while( hf_itr != hardfork_version_votes.end() )
      {
        if( hf_itr->second >= wso.hardfork_required_witnesses )
        {
          const auto& hfp = db.get_hardfork_property_object();
          if( hfp.next_hardfork != std::get<0>( hf_itr->first ) ||
            hfp.next_hardfork_time != std::get<1>( hf_itr->first ) ) {

            db.modify( hfp, [&]( hardfork_property_object& hpo )
            {
              hpo.next_hardfork = std::get<0>( hf_itr->first );
              hpo.next_hardfork_time = std::get<1>( hf_itr->first );
            } );
          }
          break;
        }

        ++hf_itr;
      }

      // We no longer have a majority
      if( hf_itr == hardfork_version_votes.end() )
      {
        db.modify( db.get_hardfork_property_object(), [&]( hardfork_property_object& hpo )
        {
          hpo.next_hardfork = hpo.current_hardfork_version;
        });
      }
    }
  }

  assert( num_elected + num_miners + num_timeshare == active_witnesses.size() );

  db.modify( wso, [&]( witness_schedule_object& _wso )
  {
    for( size_t i = 0; i < active_witnesses.size(); i++ )
    {
      _wso.current_shuffled_witnesses[i] = active_witnesses[i];
    }

    for( size_t i = active_witnesses.size(); i < HIVE_MAX_WITNESSES; i++ )
    {
      _wso.current_shuffled_witnesses[i] = account_name_type();
    }

    _wso.num_scheduled_witnesses = std::max< uint8_t >( active_witnesses.size(), 1 );
    _wso.witness_pay_normalization_factor =
        _wso.elected_weight * num_elected
      + _wso.miner_weight * num_miners
      + _wso.timeshare_weight * num_timeshare;

    /// shuffle current shuffled witnesses
    auto now_hi = uint64_t(db.head_block_time().sec_since_epoch()) << 32;
    for( uint32_t i = 0; i < _wso.num_scheduled_witnesses; ++i )
    {
      /// High performance random generator
      /// http://xorshift.di.unimi.it/
      uint64_t k = now_hi + uint64_t(i)*2685821657736338717ULL;
      k ^= (k >> 12);
      k ^= (k << 25);
      k ^= (k >> 27);
      k *= 2685821657736338717ULL;

      uint32_t jmax = _wso.num_scheduled_witnesses - i;
      uint32_t j = i + k%jmax;
      std::swap(_wso.current_shuffled_witnesses[i],
                _wso.current_shuffled_witnesses[j]);
    }

    _wso.current_virtual_time = new_virtual_time;
    _wso.next_shuffle_block_num = db.head_block_num() + _wso.num_scheduled_witnesses;
    _wso.majority_version = majority_version;
  } );

  update_median_witness_props(db, wso);
} FC_CAPTURE_AND_RETHROW() }


/**
  *
  *  See @ref witness_object::virtual_last_update
  */
void update_witness_schedule(database& db)
{ try {
  if( (db.head_block_num() % HIVE_MAX_WITNESSES) == 0 ) //wso.next_shuffle_block_num )
  {
    const witness_schedule_object& wso = db.get_witness_schedule_object();
    if( db.has_hardfork(HIVE_HARDFORK_0_4) )
    {
      if (db.has_hardfork(HIVE_HARDFORK_1_26_FUTURE_WITNESS_SCHEDULE))
      {
        //dlog("Has hardfork 1_26, generating a future shuffled witness schedule");

        // if this is the first time we've run after the hardfork, the `future_witness_schedule_object`
        // that was created during hardfork activation will be a copy of `current_shuffled_witnesses`.
        //
        // every time after that, `future_witness_schedule_object` will already have the next HIVE_MAX_WITNESSES ready,
        // so we should swap that into `current_shuffled_witnesses` and then compute the new set into 
        // `future_witness_schedule_object`
        const witness_schedule_object& future_wso = db.get_future_witness_schedule_object();

        // promote future witnesses to current
        db.modify(wso, [&](witness_schedule_object& witness_schedule)
        {
          witness_schedule.copy_values_from(future_wso);
        } );

        update_witness_schedule4(db, future_wso);
      }
      else
      {
        update_witness_schedule4(db, wso);
      }
      if( db.has_hardfork( HIVE_HARDFORK_0_20 ) )
        db.rc.set_pool_params( wso );
      return;
    }

    const auto& props = db.get_dynamic_global_properties();

    vector<account_name_type> active_witnesses;
    active_witnesses.reserve( HIVE_MAX_WITNESSES );

    fc::uint128 new_virtual_time = 0;

    /// only use vote based scheduling after the first 1M HIVE is created or if there is no POW queued
    if( props.num_pow_witnesses == 0 || db.head_block_num() > HIVE_START_MINER_VOTING_BLOCK )
    {
      const auto& widx = db.get_index<witness_index>().indices().get<by_vote_name>();

      for( auto itr = widx.begin(); itr != widx.end() && (active_witnesses.size() < (HIVE_MAX_WITNESSES-2)); ++itr )
      {
        if( itr->pow_worker )
          continue;

        active_witnesses.push_back(itr->owner);

        /// don't consider the top 19 for the purpose of virtual time scheduling
        db.modify( *itr, [&]( witness_object& wo )
        {
          wo.virtual_scheduled_time = fc::uint128_max_value();
        } );
      }

      /// add the virtual scheduled witness, reseeting their position to 0 and their time to completion

      const auto& schedule_idx = db.get_index<witness_index>().indices().get<by_schedule_time>();
      auto sitr = schedule_idx.begin();
      while( sitr != schedule_idx.end() && sitr->pow_worker )
        ++sitr;

      if( sitr != schedule_idx.end() )
      {
        active_witnesses.push_back(sitr->owner);
        db.modify( *sitr, [&]( witness_object& wo )
        {
          wo.virtual_position = fc::uint128();
          new_virtual_time = wo.virtual_scheduled_time; /// everyone advances to this time

          /// extra cautious sanity check... we should never end up here if witnesses are
          /// properly voted on. TODO: remove this line if it is not triggered and therefore
          /// the code path is unreachable.
          if( new_virtual_time == fc::uint128_max_value() )
              new_virtual_time = fc::uint128();

          /// this witness will produce again here
          if( db.has_hardfork( HIVE_HARDFORK_0_2 ) )
            wo.virtual_scheduled_time += HIVE_VIRTUAL_SCHEDULE_LAP_LENGTH2 / (wo.votes.value+1);
          else
            wo.virtual_scheduled_time += HIVE_VIRTUAL_SCHEDULE_LAP_LENGTH / (wo.votes.value+1);
        } );
      }
    }

    /// Add the next POW witness to the active set if there is one...
    const auto& pow_idx = db.get_index<witness_index>().indices().get<by_pow>();

    auto itr = pow_idx.lower_bound(1);
    /// if there is more than 1 POW witness, then pop the first one from the queue...
    if( props.num_pow_witnesses > HIVE_MAX_WITNESSES )
    {
      if( itr != pow_idx.end() )
      {
        db.modify( *itr, [&](witness_object& wit )
        {
          wit.pow_worker = 0;
        } );
        db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& obj )
        {
            obj.num_pow_witnesses--;
        } );
      }
    }

    /// add all of the pow witnesses to the round until voting takes over, then only add one per round
    itr = pow_idx.lower_bound(1);
    while( itr != pow_idx.end() )
    {
      active_witnesses.push_back( itr->owner );

      if( db.head_block_num() > HIVE_START_MINER_VOTING_BLOCK || active_witnesses.size() >= HIVE_MAX_WITNESSES )
        break;
      ++itr;
    }

    db.modify( wso, [&]( witness_schedule_object& _wso )
    {
    /*
      _wso.current_shuffled_witnesses.clear();
      _wso.current_shuffled_witnesses.reserve( active_witnesses.size() );

      for( const string& w : active_witnesses )
        _wso.current_shuffled_witnesses.push_back( w );
        */
      // active witnesses has exactly HIVE_MAX_WITNESSES elements, asserted above
      for( size_t i = 0; i < active_witnesses.size(); i++ )
      {
        _wso.current_shuffled_witnesses[i] = active_witnesses[i];
      }

      for( size_t i = active_witnesses.size(); i < HIVE_MAX_WITNESSES; i++ )
      {
        _wso.current_shuffled_witnesses[i] = account_name_type();
      }

      _wso.num_scheduled_witnesses = std::max< uint8_t >( active_witnesses.size(), 1 );

      //idump( (_wso.current_shuffled_witnesses)(active_witnesses.size()) );

      auto now_hi = uint64_t(db.head_block_time().sec_since_epoch()) << 32;
      for( uint32_t i = 0; i < _wso.num_scheduled_witnesses; ++i )
      {
        /// High performance random generator
        /// http://xorshift.di.unimi.it/
        uint64_t k = now_hi + uint64_t(i)*2685821657736338717ULL;
        k ^= (k >> 12);
        k ^= (k << 25);
        k ^= (k >> 27);
        k *= 2685821657736338717ULL;

        uint32_t jmax = _wso.num_scheduled_witnesses - i;
        uint32_t j = i + k%jmax;
        std::swap( _wso.current_shuffled_witnesses[i],
                _wso.current_shuffled_witnesses[j] );
      }

      if( props.num_pow_witnesses == 0 || db.head_block_num() > HIVE_START_MINER_VOTING_BLOCK )
        _wso.current_virtual_time = new_virtual_time;

      _wso.next_shuffle_block_num = db.head_block_num() + _wso.num_scheduled_witnesses;
    } );
    update_median_witness_props(db, wso);
  }
} FC_CAPTURE_AND_RETHROW() }

} }
