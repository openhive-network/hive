#pragma once

#include <hive/protocol/types.hpp>
#include <hive/utilities/plugin_utilities.hpp>

#include <boost/container/flat_map.hpp>
#include <boost/program_options.hpp>

namespace hive { namespace plugins {

using protocol::account_name_type;

class data_filter
{
  public:

    using account_name_range_index = flat_map< account_name_type, account_name_type >;

  private:

    account_name_range_index tracked_accounts;

  public:

    bool empty() const;
    const account_name_range_index& get_tracked_accounts() const;

    void fill( const boost::program_options::variables_map& options, const std::string& option_name );
    bool is_tracked_account( const account_name_type& name ) const;
};

} } // hive::plugins
