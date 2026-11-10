#pragma once

#include <hive/chain/external_storage/comment_rocksdb_objects.hpp>

#include <hive/protocol/transaction.hpp>

#include <hive/utilities/benchmark_dumper.hpp>

#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace hive { namespace chain {

/// Author and permlink of a comment that an operation looks up.
struct comment_lookup_key
{
  account_name_type author;
  std::string       permlink;
};

/// Appends the comments the evaluators of the transaction's operations look up by author and permlink.
void collect_comment_lookup_keys( const hive::protocol::signed_transaction& transaction, std::vector<comment_lookup_key>& keys );

/// The path the comment lookups made by the evaluator of the operation are counted under in the archive stats.
hive::utilities::benchmark_dumper::comment_lookup_path get_evaluator_lookup_path( const hive::protocol::operation& op );

/// Archive entries read ahead of their use, keyed by author and permlink hash. Thread safe.
/// Holds at most `capacity` entries and evicts the ones stored or refreshed longest ago.
/// Only entries that exist in the archive are stored: an archived entry never changes, so a
/// stored one stays valid until the archive as a whole is replaced (then the cache is cleared).
class prefetched_comment_cache
{
  public:

    using hash_type = comment_object::author_and_permlink_hash_type;

    explicit prefetched_comment_cache( size_t capacity );

    /// Removes from `hashes` the ones already stored, refreshing them as if stored again.
    void keep_missing( std::vector<hash_type>& hashes );
    void store( const std::vector<std::pair<hash_type, rocksdb_comment_object>>& entries );
    std::optional<rocksdb_comment_object> find( const hash_type& hash ) const;
    void clear();

  private:

    struct slot
    {
      rocksdb_comment_object entry;
      uint64_t               serial = 0;
    };

    void refresh( const hash_type& hash, slot& s );
    void evict();

    const size_t                                     capacity;
    mutable std::mutex                               mutex;
    std::unordered_map<hash_type, slot>              entries;
    std::deque<std::pair<hash_type, uint64_t>>       order; // oldest first; an item is stale when its serial no longer matches
    uint64_t                                         next_serial = 0;
};

/// Account ids of comment authors by name, for threads that cannot read the state. Thread safe.
/// Filled by the threads that may read the state. Any id it holds is only a guess: a wrong one
/// just makes a lookup key that the chain thread never asks for. Once it holds `capacity` names
/// it starts over.
class author_id_cache
{
  public:

    explicit author_id_cache( size_t capacity );

    /// Remembers the author unless already seen; `get_name` is called only for an unseen id.
    void remember( const account_id_type& author, const std::function<account_name_type()>& get_name );
    std::optional<account_id_type> find( const account_name_type& name ) const;
    void clear();

  private:

    const size_t                                          capacity;
    mutable std::mutex                                    mutex;
    std::unordered_map<account_name_type, account_id_type> ids;
    std::vector<bool>                                     seen; // indexed by account id
};

} } // hive::chain
