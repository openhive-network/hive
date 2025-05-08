#pragma once

#include <core/utilities.hpp>

#include <string>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/ordered_index.hpp>

namespace beekeeper {

  using boost::multi_index::multi_index_container;
  using boost::multi_index::indexed_by;
  using boost::multi_index::ordered_non_unique;
  using boost::multi_index::ordered_unique;
  using boost::multi_index::tag;
  using boost::multi_index::member;

class time_manager_base
{
  public:

    using ptr_time_manager_base = std::shared_ptr<time_manager_base>;

  private:

    struct session_data
    {
      std::string token;
      types::lock_method_type lock_method;
      types::timepoint_t time = types::timepoint_t::max();
    };

    struct by_time;
    struct by_token;

    typedef multi_index_container<
      session_data,
      indexed_by<
        ordered_non_unique< tag< by_time >,
          member< session_data, types::timepoint_t, &session_data::time > >,
        ordered_unique< tag< by_token >,
          member< session_data, std::string, &session_data::token > >
      >
    > session_data_index;

    session_data_index items;

    bool run( const types::timepoint_t& now, const session_data& s_data, std::vector<std::string>& modified_items );
    void modify_times( const std::vector<std::string>& modified_items );

  public:

    virtual ~time_manager_base(){}

    virtual void add( const std::string& token, types::lock_method_type&& lock_method );
    virtual void change( const std::string& token, const types::timepoint_t& time, bool refresh_only_active );

    virtual void run();
    virtual void run( const std::string& token );

    virtual void close( const std::string& token );
};

} //beekeeper