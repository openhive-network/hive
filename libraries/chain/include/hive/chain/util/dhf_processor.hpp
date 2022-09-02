#pragma once

#include <hive/chain/dhf_objects.hpp>
#include <hive/chain/notifications.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/index.hpp>
#include <hive/chain/account_object.hpp>

#include <hive/chain/util/dhf_helper.hpp>

#include <hive/protocol/dhf_operations.hpp>

namespace hive { namespace chain {

class dhf_processor
{
  public:

    using t_proposals = std::vector< std::reference_wrapper< const proposal_object > >;

  private:

    const static std::string removing_name;
    const static std::string calculating_name;
    const static uint32_t total_amount_divider = 100;

    //Get number of microseconds for 1 day( daily_ms )
    const int64_t daily_seconds = fc::days(1).to_seconds();

    chain::database& db;

    bool is_maintenance_period( const time_point_sec& head_time ) const;
    bool is_daily_maintenance_period( const time_point_sec& head_time ) const;

    void remove_proposals( const time_point_sec& head_time );

    void find_proposals( const time_point_sec& head_time, t_proposals& active_proposals, t_proposals& no_active_yet_proposals );

    uint64_t calculate_votes( uint32_t pid );
    void calculate_votes( const t_proposals& proposals );

    void sort_by_votes( t_proposals& proposals );

    asset get_treasury_fund();

    asset calculate_maintenance_budget( const time_point_sec& head_time );

    void transfer_payments( const time_point_sec& head_time, asset& maintenance_budget_limit, const t_proposals& proposals );

    void update_settings( const time_point_sec& head_time );

    void remove_old_proposals( const block_notification& note );
    void make_payments( const block_notification& note );

    void record_funding( const block_notification& note );
    void convert_funds( const block_notification& note );

  public:

    dhf_processor( chain::database& _db ) : db( _db ){}

    const static std::string& get_removing_name();
    const static std::string& get_calculating_name();

    void run( const block_notification& note );
};

} } // namespace hive::chain
