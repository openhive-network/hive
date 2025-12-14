#include <hive/chain/witness_objects_multiindex.hpp>
#include <hive/chain/account_object_multiindex.hpp>

#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

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

  if( has_hardfork( HIVE_HARDFORK_0_6__104 ) )
    modify( a, [&](account_object& acc )
    {
      acc.witnesses_voted_for = 0;
    });
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

    const auto& vidx = get_index<witness_vote_index>().indices().get<by_account_witness>();
    auto wit_itr = vidx.lower_bound( boost::make_tuple( a.get_name(), account_name_type() ) );
    while( wit_itr != vidx.end() && wit_itr->account == a.get_name() )
    {
      adjust_witness_vote( get< witness_object, by_name >(wit_itr->witness), a.get_governance_vote_power() );
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
    if( a.witnesses_voted_for != witnesses_voted_for )
    {
      modify( a, [&]( account_object& account )
      {
        account.witnesses_voted_for = witnesses_voted_for;
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