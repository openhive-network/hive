
#pragma once

#include <hive/chain/external_storage/external_storage_snapshot.hpp>
#include <hive/chain/external_storage/comment.hpp>

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/comment_object.hpp>

#include <hive/utilities/benchmark_dumper.hpp>

namespace hive { namespace chain {

class comments_handler : public external_storage_snapshot
{
  public:

    using ptr = std::shared_ptr<comments_handler>;

    virtual void on_cashout( const comment_object& _comment, const comment_cashout_object& _comment_cashout ) = 0;
    virtual void on_irreversible_block( uint32_t block_num ) = 0;

    virtual comment get_comment( const account_id_type& author, const std::string& permlink, bool comment_is_required ) const = 0;

    virtual void open() = 0;
    virtual void close() = 0;
    virtual void wipe() = 0;

    virtual void update_lib( uint32_t ) = 0;
    virtual void update_reindex_point( uint32_t ) = 0;

    static hive::utilities::benchmark_dumper::comment_archive_details_t stats; // note: times should be measured in nanoseconds
};

} } // hive::chain
