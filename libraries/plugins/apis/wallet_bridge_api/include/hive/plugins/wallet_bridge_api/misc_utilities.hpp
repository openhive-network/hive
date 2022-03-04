#pragma once

#include <fc/variant.hpp>

#include <hive/protocol/asset.hpp>
#include <hive/plugins/wallet_bridge_api/wallet_bridge_api_args.hpp>

#include<vector>
#include<string>

namespace hive { namespace plugins { namespace wallet_bridge_api {

using method_description_vector = vector<method_description>;

typedef boost::multi_index::multi_index_container<method_description,
  boost::multi_index::indexed_by<
    boost::multi_index::ordered_unique<
      boost::multi_index::member<method_description, std::string, &method_description::method_name> > > > method_description_set;

class api_documentation_reader
{
  method_description_set method_descriptions;

  public:

    void fill_help( const vector<method_description>& help )
    {
      if( method_descriptions.size() > 0 )
        return;

      for( auto& item : help )
      {
        method_descriptions.emplace( item );
      }
    }

    std::string get_brief_description(const std::string& method_name) const
    {
      auto iter = method_descriptions.find(method_name);
      if (iter != method_descriptions.end())
        return iter->brief_description;
      else
        FC_THROW_EXCEPTION(fc::key_not_found_exception, "No entry for method ${name}", ("name", method_name));
    }

    std::string get_detailed_description(const std::string& method_name) const
    {
      auto iter = method_descriptions.find(method_name);
      if (iter != method_descriptions.end())
        return iter->detailed_description;
      else
        FC_THROW_EXCEPTION(fc::key_not_found_exception, "No entry for method ${name}", ("name", method_name));
    }

    std::vector<std::string> get_method_names() const
    {
      std::vector<std::string> method_names;
      for (const method_description& method_description: method_descriptions)
        method_names.emplace_back(method_description.method_name);
      return method_names;
    }
};

class wallet_formatter
{
    api_documentation_reader method_documentation;

  public:

    wallet_formatter()
    {
    }

    template<typename T>
    variant get_result( const std::stringstream& out_text, const T& out_json, format_type format );

    variant help( format_type format );
    variant gethelp( const string& method, format_type format );
    variant fill_help( const vector<method_description>& help );
    variant list_my_accounts( const serializer_wrapper<vector<database_api::api_account_object>>& accounts, format_type format );
    string get_account_history( variant result );
    string get_open_orders( variant result );
    string get_order_book( variant result );
    string get_withdraw_routes( variant result );
};

} } } //hive::plugins::wallet_bridge_api
