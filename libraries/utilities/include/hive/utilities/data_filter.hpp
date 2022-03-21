#pragma once

#include <hive/protocol/types.hpp>
#include <hive/protocol/operations.hpp>

#include <hive/utilities/plugin_utilities.hpp>

#include <boost/container/flat_map.hpp>
#include <boost/program_options.hpp>

namespace hive { namespace plugins {

using protocol::account_name_type;
using protocol::operation;
using std::string;

class data_filter
{
  protected:

    string filter_name;

  public:
  
    data_filter( const string& _filter_name );
};

class account_filter: public data_filter
{
  public:

    using account_name_range_index = flat_map< account_name_type, account_name_type >;

  private:

    account_name_range_index tracked_accounts;

  public:

    account_filter( const string& _filter_name );

    bool empty() const;
    const account_name_range_index& get_tracked_accounts() const;
    bool is_tracked_account( const account_name_type& name ) const;

    void fill( const boost::program_options::variables_map& options, const string& option_name );
};

struct operation_helper
{
  static bool create( const string& name, operation& op );
  static string get_op_name( const operation& op );
};

class operation_filter: public data_filter
{
  public:

    using operations = flat_set<operation>;

  private:

    operations tracked_operations;

  public:

    operation_filter( const string& _filter_name );

    bool empty() const;
    bool is_tracked_operation( const operation& op ) const;

    void fill( const boost::program_options::variables_map& options, const string& option_name );
};

class operation_body_filter: public data_filter
{
  public:

    using body_filters_items = flat_map< operation, string >;

  private:

    body_filters_items body_filters;

  public:

    operation_body_filter( const string& _filter_name );

    bool empty() const;
    bool is_tracked_operation( const operation& op ) const;

    void fill( const boost::program_options::variables_map& options, const string& option_name );
};

} } // hive::plugins
