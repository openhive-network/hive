#include <hive/utilities/data_filter.hpp>
#include <hive/utilities/plugin_utilities.hpp>

#include <boost/algorithm/string.hpp>

#define DIAGNOSTIC(s)
//#define DIAGNOSTIC(s) s

namespace hive { namespace plugins {

data_filter::data_filter( const std::string& _filter_name ): filter_name( _filter_name )
{

}

account_filter::account_filter( const std::string& _filter_name ): data_filter( _filter_name )
{
}

bool account_filter::empty() const
{
  return tracked_accounts.empty();
}

const account_filter::account_name_range_index& account_filter::get_tracked_accounts() const
{
  return tracked_accounts;
}

bool account_filter::is_tracked_account( const account_name_type& name ) const
{
  if( empty() )
  {
    DIAGNOSTIC( ilog("[${fn}] Set of tracked accounts is empty.", ("fn", filter_name) ); )
    return true;
  }

  /// Code below is based on original contents of account_history_plugin_impl::on_pre_apply_operation
  auto itr = tracked_accounts.lower_bound(name);

  /*
    * The map containing the ranges uses the key as the lower bound and the value as the upper bound.
    * Because of this, if a value exists with the range (key, value], then calling lower_bound on
    * the map will return the key of the next pair. Under normal circumstances of those ranges not
    * intersecting, the value we are looking for will not be present in range that is returned via
    * lower_bound.
    *
    * Consider the following example using ranges ["a","c"], ["g","i"]
    * If we are looking for "bob", it should be tracked because it is in the lower bound.
    * However, lower_bound( "bob" ) returns an iterator to ["g","i"]. So we need to decrement the iterator
    * to get the correct range.
    *
    * If we are looking for "g", lower_bound( "g" ) will return ["g","i"], so we need to make sure we don't
    * decrement.
    *
    * If the iterator points to the end, we should check the previous (equivalent to rbegin)
    *
    * And finally if the iterator is at the beginning, we should not decrement it for obvious reasons
    */
  if(itr != tracked_accounts.begin() &&
    ((itr != tracked_accounts.end() && itr->first != name ) || itr == tracked_accounts.end()))
  {
    --itr;
  }

  bool _result = itr != tracked_accounts.end() && itr->first <= name && name <= itr->second;

  DIAGNOSTIC
  (
    if( _result )
      ilog("[${fn}] Account: ${a} matched to defined account range: [${b},${e}]", ("fn", filter_name)("a", name)("b", itr->first)("e", itr->second) );
    else
      ilog("[${fn}] Account: ${a} ignored due to defined tracking filters.", ("fn", filter_name)("a", name));
  )

  return _result;
}

void account_filter::fill( const boost::program_options::variables_map& options, const std::string& option_name )
{
  typedef std::pair< account_name_type, account_name_type > pairstring;
  HIVE_LOAD_VALUE_SET(options, option_name, tracked_accounts, pairstring);
}

operation_filter::operation_filter( const std::string& _filter_name ): data_filter( _filter_name )
{
}

std::string operation_filter::get_op_name( const operation& op )
{
  string _op_name;
  op.visit( fc::get_static_variant_name( _op_name ) );

  return _op_name;
}

bool operation_filter::add_operation( const std::string& name, operation& op )
{
  static std::map< string, int64_t > _ops = fc::prepare_variant_info<operation>();

  auto itr = _ops.find( name );

  if( itr != _ops.end() )
  {
    int64_t which = itr->second;
    op.set_which( which );

    return true;
  }
  else
    return false;
}

bool operation_filter::empty() const
{
  return tracked_operations.empty();
}

bool operation_filter::is_tracked_operation( const operation& op ) const
{
  if( empty() )
  {
    DIAGNOSTIC( ilog("[${fn}] Set of tracked operations is empty.", ("fn", filter_name) ); )
    return true;
  }

  bool _result = tracked_operations.find( op ) != tracked_operations.end();

  DIAGNOSTIC
  (
    if( _result )
      ilog("[${fn}] Operation: ${op} matched to defined operations", ("fn", filter_name)("op", get_op_name( op )) );
    else
      ilog("[${fn}] Operation: ${op} ignored due to defined tracking filters.", ("fn", filter_name)("op", get_op_name( op )) );
  )

  return _result;
}

void operation_filter::fill( const boost::program_options::variables_map& options, const std::string& option_name )
{
  if( options.count( option_name ) > 0 )
  {
    flat_set<std::string> _ops;

    auto _set_of_ops = options.at( option_name ).as<std::vector<std::string>>();
    for( auto& item : _set_of_ops )
    {
      vector<string> _names;
      boost::split( _names, item, boost::is_any_of(" \t,") );

      std::copy( _names.begin(), _names.end(), std::inserter( _ops, _ops.begin() ) );
    }

    for( auto& item : _ops )
    {
      operation op;
      if( add_operation( item, op ) )
      {
        tracked_operations.emplace( op );
      }
    }
  }
}

} } // hive::utilities
