#pragma once

#include <mutex>
#include <thread>

#include <core/time_manager_base.hpp>

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

class time_manager: public time_manager_base
{
  private:

    struct session_data
    {
      std::string token;
      types::lock_method_type lock_method;
      types::notification_method_type notification_method;
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

    bool stop_requested = false;

    std::unique_ptr<std::thread> notification_thread;

    std::mutex methods_mutex;

    session_data_index items;

  public:

    time_manager();
    ~time_manager() override;

    void add( const std::string& token, types::lock_method_type&& lock_method, types::notification_method_type&& notification_method ) override;
    void change( const std::string& token, const types::timepoint_t& time ) override;

    void run() override;

    void close( const std::string& token ) override;
};

} //beekeeper
