#include <hive/utilities/data_filter.hpp>
#include <hive/utilities/plugin_utilities.hpp>

//#define DIAGNOSTIC(s)
#define DIAGNOSTIC(s) s

namespace hive { namespace plugins {

data_filter::data_filter( const std::string& _filter_name ): filter_name( _filter_name )
{
}

bool data_filter::empty() const
{
  return tracked_accounts.empty();
}

const data_filter::account_name_range_index& data_filter::get_tracked_accounts() const
{
  return tracked_accounts;
}

void data_filter::fill( const boost::program_options::variables_map& options, const std::string& option_name )
{
  typedef std::pair< account_name_type, account_name_type > pairstring;
  HIVE_LOAD_VALUE_SET(options, option_name, tracked_accounts, pairstring);
}

bool data_filter::is_tracked_account( const account_name_type& name ) const
{
  if( tracked_accounts.empty() )
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

} } // hive::utilities
