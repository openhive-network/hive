#pragma once

#include <hive/chain/full_transaction.hpp>

#include <memory>
#include <vector>

namespace hive { namespace chain {

/// Reads the archived comments that transactions are going to look up before the chain thread
/// applies them. Called from the worker threads; calls never change state and may be dropped.
class comment_archive_prefetcher
{
  public:

    using ptr = std::shared_ptr<comment_archive_prefetcher>;

    virtual ~comment_archive_prefetcher() = default;

    virtual void prefetch_comments( const std::vector<full_transaction_ptr>& transactions ) = 0;
};

} } // hive::chain
