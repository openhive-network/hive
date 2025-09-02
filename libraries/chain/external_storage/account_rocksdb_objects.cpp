#include <hive/chain/account_object.hpp>
#include <hive/chain/external_storage/account_rocksdb_objects.hpp>

namespace hive { namespace chain {

hive::utilities::benchmark_dumper::account_archive_details_t accounts_stats::stats;

std::shared_ptr<account_object> volatile_account_object::read() const
{
  return std::shared_ptr<account_object>( new account_object(
                                                  account_id,
                                                  recovery,
                                                  assets,
                                                  mrc,
                                                  time,
                                                  misc,
                                                  shared_delayed_votes) );
}

}}