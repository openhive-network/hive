#pragma once

#include <hive/chain/external_storage/comments_handler.hpp>

namespace hive { namespace chain {

class database;

class placeholder_comment_archive final : public comments_handler
{
  private:
    database& db;

  public:
    placeholder_comment_archive( database& db ) : db( db ) {}
    virtual ~placeholder_comment_archive() {}

    virtual void on_cashout( const comment_object& _comment, const comment_cashout_object& _comment_cashout ) override;
    virtual void on_irreversible_block( uint32_t block_num ) override;

    virtual comment get_comment( const account_id_type& author, const std::string& permlink, bool comment_is_required ) const override;

    virtual void save_snaphot( const prepare_snapshot_supplement_notification& note ) override;
    virtual void load_snapshot( const load_snapshot_supplement_notification& note ) override;

    virtual void open() override;
    virtual void close() override;
    virtual void wipe() override;
};

} }
