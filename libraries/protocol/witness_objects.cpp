#include <hive/protocol/witness_objects.hpp>

namespace hive { namespace protocol {

void extract_set_witness_properties(
  const witness_set_properties_props_t& input,
  witness_properties_change_flags& flags,
  witness_set_properties_inputs& properties,
  const bool has_hardfork_24,
  const post_set_witness_properties_function_t post_callback
)
{
  using namespace std::placeholders;
  using iterator_t = witness_set_properties_props_t::const_iterator;
  auto callback = [&input, &post_callback](const iterator_t& it, const witness_set_properties_input_compose_t& obj){ post_callback(it->first, it != input.end(), obj); };

  iterator_t itr = input.find( "key" );
  fc::raw::unpack_from_vector( itr->second, properties.signing_key );
  callback(itr, properties.signing_key);

  itr = input.find( "account_creation_fee" );
  flags.account_creation_changed = itr != input.end();
  if( flags.account_creation_changed )
  {
    fc::raw::unpack_from_vector( itr->second, properties.account_creation_fee );
    callback(itr, properties.account_creation_fee);
  }

  itr = input.find( "maximum_block_size" );
  flags.max_block_changed = itr != input.end();
  if( flags.max_block_changed )
  {
    fc::raw::unpack_from_vector( itr->second, properties.maximum_block_size );
    callback(itr, properties.maximum_block_size);
  }

  itr = input.find( "sbd_interest_rate" );
  if(itr == input.end() && has_hardfork_24)
    itr = input.find( "hbd_interest_rate" );

  flags.hbd_interest_changed = itr != input.end();
  if( flags.hbd_interest_changed )
  {
    fc::raw::unpack_from_vector( itr->second, properties.hbd_interest_rate );
    callback(itr, properties.hbd_interest_rate);
  }

  itr = input.find( "account_subsidy_budget" );
  flags.account_subsidy_budget_changed = itr != input.end();
  if( flags.account_subsidy_budget_changed )
  {
    fc::raw::unpack_from_vector( itr->second, properties.account_subsidy_budget );
    callback(itr, properties.account_subsidy_budget);
  }

  itr = input.find( "account_subsidy_decay" );
  flags.account_subsidy_decay_changed = itr != input.end();
  if( flags.account_subsidy_decay_changed )
  {
    fc::raw::unpack_from_vector( itr->second, properties.account_subsidy_decay );
    callback(itr, properties.account_subsidy_decay);
  }

  itr = input.find( "new_signing_key" );
  flags.key_changed = itr != input.end();
  if( flags.key_changed )
  {
    fc::raw::unpack_from_vector( itr->second, properties.signing_key );
    callback(itr, properties.signing_key);
  }

  itr = input.find( "sbd_exchange_rate" );
  if(itr == input.end() && has_hardfork_24)
    itr = input.find("hbd_exchange_rate");

  flags.hbd_exchange_changed = itr != input.end();
  if( flags.hbd_exchange_changed )
  {
    fc::raw::unpack_from_vector( itr->second, properties.hbd_exchange_rate );
    callback(itr, properties.hbd_exchange_rate);
  }

  itr = input.find( "url" );
  flags.url_changed = itr != input.end();
  if( flags.url_changed )
  {
    fc::raw::unpack_from_vector< std::string >( itr->second, properties.url );
    callback(itr, properties.url);
  }
}


} } // hive::protocol
