
#pragma once

#include <hive/chain/external_storage/external_storage_snapshot.hpp>
#include <hive/chain/external_storage/comment.hpp>
#include <hive/chain/external_storage/comments_handler_ptr.hpp>

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/detail/state/comment_object.hpp>

#include <hive/utilities/benchmark_dumper.hpp>

namespace hive { namespace chain {

class comments_handler : public external_storage_snapshot
{
  public:

    using ptr = comments_handler_ptr;
    using lookup_path = hive::utilities::benchmark_dumper::comment_lookup_path;

    /// Attributes the comment lookups made on the current thread while it lives to `path` in `stats`.
    class lookup_path_scope
    {
      public:
        explicit lookup_path_scope( lookup_path path ) : previous( current_lookup_path ) { current_lookup_path = path; }
        ~lookup_path_scope() { current_lookup_path = previous; }
        lookup_path_scope( const lookup_path_scope& ) = delete;
        lookup_path_scope& operator=( const lookup_path_scope& ) = delete;

      private:
        const lookup_path previous;
    };

    virtual void on_cashout( uint32_t _block_num, const comment_object& _comment, const comment_cashout_object& _comment_cashout ) = 0;
    virtual void on_irreversible_block( uint32_t block_num ) = 0;

    virtual comment get_comment( const account_id_type& author, const std::string& permlink, bool comment_is_required ) const = 0;

    virtual void open() = 0;
    virtual void close() = 0;
    virtual void flush( bool force_storage_flush ) = 0;
    virtual void wipe() = 0;

    virtual void on_end_of_syncing() = 0;
    /// The state storage was mapped again, possibly at another address.
    virtual void on_segment_remapped() = 0;

    static hive::utilities::benchmark_dumper::comment_archive_details_t stats; // note: times should be measured in nanoseconds

  protected:

    static hive::utilities::benchmark_dumper::comment_lookup_path_counters_t& current_path_stats()
    {
      return stats.lookups_by_path[ static_cast<size_t>( current_lookup_path ) ];
    }

  private:

    static thread_local lookup_path current_lookup_path;
};

} } // hive::chain
