#include <hive/chain/witness_objects_multiindex.hpp>
#include <hive/chain/account_object_multiindex.hpp>
#include <hive/chain/global_property_object_multiindex.hpp>
#include <hive/chain/database_virtual_operations.hpp>

#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>
#include <hive/chain/util/rd_setup.hpp>
#include <hive/protocol/block.hpp>
#include <hive/protocol/hardfork.hpp>

namespace hive { namespace chain {

using hive::protocol::hardfork_version_vote;
using hive::protocol::shutdown_witness_operation;
using hive::protocol::producer_missed_operation;

void initialize_core_indexes_04( database& db )
{
  HIVE_ADD_CORE_INDEX(db, witness_index);
  HIVE_ADD_CORE_INDEX(db, witness_vote_index);
  HIVE_ADD_CORE_INDEX(db, witness_schedule_index);

}

const witness_object* database::find_witness( const account_name_type& name ) const
{
  return find< witness_object, by_name >( name );
}


const witness_schedule_object& database::get_witness_schedule_object()const
{ try {
  return get< witness_schedule_object >();
} FC_CAPTURE_AND_RETHROW() }

const witness_schedule_object& database::get_future_witness_schedule_object() const
{
  try
  {
    return get<witness_schedule_object>(witness_schedule_object::id_type(1));
  }
  catch (const std::out_of_range&)
  {
    FC_THROW_EXCEPTION(fc::key_not_found_exception, "Future witness schedule does not exist");
  }
}

const witness_object& database::get_witness( const account_name_type& name ) const
{ try {
  const auto* _witness = find_witness( name );
  FC_ASSERT( _witness != nullptr, "Witness ${w} doesn't exist", ("w", name) );
  return *_witness;
} FC_CAPTURE_AND_RETHROW( (name) ) }

void database::adjust_witness_votes( const account_object& a, const share_type& delta )
{
  const auto& vidx = get_index< witness_vote_index >().indices().get< by_account_witness >();
  auto itr = vidx.lower_bound( boost::make_tuple( a.get_name(), account_name_type() ) );
  while( itr != vidx.end() && itr->account == a.get_name() )
  {
    adjust_witness_vote( get< witness_object, by_name >(itr->witness), delta );
    ++itr;
  }
}

void database::adjust_witness_vote( const witness_object& witness, share_type delta )
{
  const witness_schedule_object& wso = has_hardfork(HIVE_HARDFORK_1_27_FIX_TIMESHARE_WITNESS_SCHEDULING) ?
                                       get_witness_schedule_object_for_irreversibility() : get_witness_schedule_object();
  modify( witness, [&]( witness_object& w )
  {
    auto delta_pos = w.votes.value * (wso.current_virtual_time - w.virtual_last_update);
    w.virtual_position += delta_pos;

    w.virtual_last_update = wso.current_virtual_time;
    w.votes += delta;
    FC_ASSERT( w.votes <= get_dynamic_global_properties().total_vesting_shares.amount, "", ("w.votes", w.votes)("props",get_dynamic_global_properties().total_vesting_shares) );

    if( has_hardfork( HIVE_HARDFORK_0_2 ) )
      w.virtual_scheduled_time = w.virtual_last_update + (HIVE_VIRTUAL_SCHEDULE_LAP_LENGTH2 - w.virtual_position)/(w.votes.value+1);
    else
      w.virtual_scheduled_time = w.virtual_last_update + (HIVE_VIRTUAL_SCHEDULE_LAP_LENGTH - w.virtual_position)/(w.votes.value+1);

    /** witnesses with a low number of votes could overflow the time field and end up with a scheduled time in the past */
    if( has_hardfork( HIVE_HARDFORK_0_4 ) )
    {
      if( w.virtual_scheduled_time < wso.current_virtual_time )
        w.virtual_scheduled_time = fc::uint128_max_value();
    }
  } );
}

void database::clear_witness_votes( const account_object& a )
{
  const auto& vidx = get_index< witness_vote_index >().indices().get<by_account_witness>();
  auto itr = vidx.lower_bound( boost::make_tuple( a.get_name(), account_name_type() ) );
  while( itr != vidx.end() && itr->account == a.get_name() )
  {
    const auto& current = *itr;
    ++itr;
    remove(current);
  }

  modify( a, [&]( account_object& acc )
  {
    acc.set_witnesses_voted_for(0);
  } );
}

void database::retally_witness_votes()
{
  const auto& witness_idx = get_index< witness_index >().indices();

  // Clear all witness votes
  for( auto itr = witness_idx.begin(); itr != witness_idx.end(); ++itr )
  {
    modify( *itr, [&]( witness_object& w )
    {
      w.votes = 0;
      w.virtual_position = 0;
    } );
  }

  const auto& account_idx = get_index< account_index >().indices();

  // Apply all existing votes by account
  for( auto itr = account_idx.begin(); itr != account_idx.end(); ++itr )
  {
    if( itr->has_proxy() ) continue;

    const auto& a = *itr;
    const auto& _assets_obj = get< assets_object, by_account_id >( a.get_id() );
    const auto& _dvotes = get< delayed_votes_object, by_account_id >( a.get_id() );

    const auto& vidx = get_index<witness_vote_index>().indices().get<by_account_witness>();
    auto wit_itr = vidx.lower_bound( boost::make_tuple( a.get_name(), account_name_type() ) );
    while( wit_itr != vidx.end() && wit_itr->account == a.get_name() )
    {
      adjust_witness_vote( get< witness_object, by_name >(wit_itr->witness), a.get_governance_vote_power( _assets_obj, _dvotes ) );
      ++wit_itr;
    }
  }
}

void database::retally_witness_vote_counts( bool force )
{
  const auto& account_idx = get_index< account_index >().indices();

  // Check all existing votes by account
  for( auto itr = account_idx.begin(); itr != account_idx.end(); ++itr )
  {
    const auto& a = *itr;
    uint16_t witnesses_voted_for = 0;
    if( force || a.has_proxy() )
    {
      const auto& vidx = get_index< witness_vote_index >().indices().get< by_account_witness >();
      auto wit_itr = vidx.lower_bound( boost::make_tuple( a.get_name(), account_name_type() ) );
      while( wit_itr != vidx.end() && wit_itr->account == a.get_name() )
      {
        ++witnesses_voted_for;
        ++wit_itr;
      }
    }
    if( a.get_witnesses_voted_for() != witnesses_voted_for )
    {
      modify( a, [&]( account_object& account )
      {
        account.set_witnesses_voted_for( witnesses_voted_for );
      } );
    }
  }
}

void database::update_signing_witness(const witness_object& signing_witness, const signed_block& new_block)
{ try {
  const dynamic_global_property_object& dpo = get_dynamic_global_properties();
  uint64_t new_block_aslot = dpo.current_aslot + get_slot_at_time( new_block.timestamp );

  modify( signing_witness, [&]( witness_object& _wit )
  {
    _wit.last_aslot = new_block_aslot;
    _wit.last_confirmed_block_num = new_block.block_num();
  } );
} FC_CAPTURE_AND_RETHROW() }

void database::update_witness_hardfork_version_votes(
  const hardfork_version& hardfork_version,
  const fc::time_point_sec& hardfork_time )
{
  const auto& witness_idx = get_index<witness_index>().indices().get<by_id>();
  vector<witness_id_type> wit_ids_to_update;
  for( auto it=witness_idx.begin(); it!=witness_idx.end(); ++it )
    wit_ids_to_update.push_back( it->get_id() );

  for( witness_id_type wit_id : wit_ids_to_update )
  {
    modify( get( wit_id ), [&]( witness_object& wit )
    {
      wit.running_version = hardfork_version;
      wit.hardfork_version_vote = hardfork_version;
      wit.hardfork_time_vote = hardfork_time;
    } );
  }
}

void database::update_witness_schedule_for_elected( const witness_object& current_witness,
  const rd_dynamics_params& account_subsidy_rd )
{
  if( current_witness.schedule == witness_object::elected )
  {
    modify( current_witness, [&]( witness_object& w )
    {
      w.available_witness_account_subsidies = rd_apply( account_subsidy_rd, w.available_witness_account_subsidies );
    } );
  }
}

struct process_header_visitor
{
  process_header_visitor( const std::string& witness, database& db ) :
    _witness( witness ),
    _db( db ) {}

  typedef void result_type;

  const std::string& _witness;
  database& _db;

  void operator()( const void_t& obj ) const
  {
    //Nothing to do.
  }

  void operator()( const version& reported_version ) const
  {
    const auto& signing_witness = _db.get_witness( _witness );
    //idump( (next_block.witness)(signing_witness.running_version)(reported_version) );

    if( reported_version != signing_witness.running_version )
    {
      _db.modify( signing_witness, [&]( witness_object& wo )
      {
        wo.running_version = reported_version;
      });
    }
  }

  void operator()( const hardfork_version_vote& hfv ) const
  {
    const auto& signing_witness = _db.get_witness( _witness );
    //idump( (next_block.witness)(signing_witness.running_version)(hfv) );

    if( hfv.hf_version != signing_witness.hardfork_version_vote || hfv.hf_time != signing_witness.hardfork_time_vote )
      _db.modify( signing_witness, [&]( witness_object& wo )
      {
        wo.hardfork_version_vote = hfv.hf_version;
        wo.hardfork_time_vote = hfv.hf_time;
      });
  }
};

void database::process_header_extensions( const signed_block& next_block )
{
  process_header_visitor _v( next_block.witness, *this );

  for( const auto& e : next_block.extensions )
    e.visit( _v );
}

uint64_t database::validate_witness_votes_invariant() const
{
  uint64_t witness_no = 0;
  const auto& gpo = get_dynamic_global_properties();
  const auto& witness_idx = get_index< witness_index >().indices();
  for( auto itr = witness_idx.begin(); itr != witness_idx.end(); ++itr )
  {
    FC_ASSERT( itr->votes <= gpo.total_vesting_shares.amount, "", ("itr",*itr) );
    ++witness_no;
  }
  return witness_no;
}

void database::update_witness_missed_blocks( const account_name_type& block_witness, uint32_t missed_blocks )
{
  for( uint32_t i = 0; i < missed_blocks; ++i )
  {
    const auto& witness_missed = get_witness( get_scheduled_witness( i + 1 ) );
    if( witness_missed.owner != block_witness )
    {
      modify( witness_missed, [&]( witness_object& w )
      {
        w.total_missed++;
        if( has_hardfork( HIVE_HARDFORK_0_14__278 ) && !has_hardfork( HIVE_HARDFORK_0_20__SP190 ) )
        {
          if( head_block_num() - w.last_confirmed_block_num > HIVE_WITNESS_SHUTDOWN_THRESHOLD )
          {
            w.signing_key = public_key_type();
            push_virtual_operation( *this, shutdown_witness_operation( w.owner ) );
          }
        }
        push_virtual_operation( *this, producer_missed_operation( w.owner ) );
      } );
    }
  }
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::witness_schedule_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::witness_vote_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::witness_index)

// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::witness_index>& chainbase::database::get_index<hive::chain::witness_index>() const;
template chainbase::generic_index<hive::chain::witness_index>& chainbase::database::get_mutable_index<hive::chain::witness_index>();

template const chainbase::generic_index<hive::chain::witness_vote_index>& chainbase::database::get_index<hive::chain::witness_vote_index>() const;
template chainbase::generic_index<hive::chain::witness_vote_index>& chainbase::database::get_mutable_index<hive::chain::witness_vote_index>();

template const chainbase::generic_index<hive::chain::witness_schedule_index>& chainbase::database::get_index<hive::chain::witness_schedule_index>() const;
template chainbase::generic_index<hive::chain::witness_schedule_index>& chainbase::database::get_mutable_index<hive::chain::witness_schedule_index>();