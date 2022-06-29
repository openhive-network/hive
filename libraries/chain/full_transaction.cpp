#include <hive/chain/full_transaction.hpp>
#include <hive/chain/full_block.hpp>
#include <hive/protocol/exceptions.hpp>
#include <hive/protocol/hardfork.hpp>
#include <boost/scope_exit.hpp>
#include <boost/lockfree/queue.hpp>
#include <mutex>

namespace fc {
  void to_variant(std::chrono::nanoseconds duration, fc::variant& var)
  {
    var = std::to_string(duration.count());
  }
}
namespace hive { namespace chain {

std::atomic<uint32_t> cached_validate_calls = {0};
std::atomic<uint32_t> non_cached_validate_calls = {0};
std::atomic<uint32_t> cached_get_signature_keys_calls = {0};
std::atomic<uint32_t> non_cached_get_signature_keys_calls = {0};
std::atomic<uint32_t> cached_get_required_authorities_calls = {0};
std::atomic<uint32_t> non_cached_get_required_authorities_calls = {0};

/* static */ std::atomic<uint32_t> full_transaction_type::number_of_instances_created = {0};
/* static */ std::atomic<uint32_t> full_transaction_type::number_of_instances_destroyed = {0};

full_transaction_type::full_transaction_type()
{
  number_of_instances_created.fetch_add(1, std::memory_order_relaxed);
  // if (number_of_instances_created.load() % 100000 == 0)
  //   ilog("Currently ${count} full_transactions in memory", ("count", number_of_instances_created.load() - number_of_instances_destroyed.load()));
}

full_transaction_type::~full_transaction_type()
{
  if (is_in_cache)
    full_transaction_cache::get_instance().remove_from_cache(get_merkle_digest());
  number_of_instances_destroyed.fetch_add(1, std::memory_order_relaxed);
}

const signed_transaction& full_transaction_type::get_transaction() const
{ 
  if (storage.which() == storage_type::tag<contained_in_block_info>::value)
  {
    const contained_in_block_info& contained_in_block = storage.get<contained_in_block_info>();
    assert(contained_in_block.block_storage->block);
    FC_ASSERT(contained_in_block.block_storage->block, "block should have already been decoded");
    return contained_in_block.block_storage->block->transactions[contained_in_block.index_in_block];
  }
  else //this is a standalone transaction
  {
    const standalone_transaction_info& standalone_transaction = storage.get<standalone_transaction_info>();
    return standalone_transaction.transaction;
  }
}

const hive::protocol::digest_type& full_transaction_type::get_merkle_digest() const
{
  if (!merkle_digest)
    merkle_digest = hive::protocol::digest_type::hash(serialized_transaction.begin, serialized_transaction.signed_transaction_end - serialized_transaction.begin);
  return *merkle_digest;
}

const hive::protocol::digest_type& full_transaction_type::get_digest() const
{
  if (!digest)
    digest = hive::protocol::digest_type::hash(serialized_transaction.begin, serialized_transaction.transaction_end - serialized_transaction.begin);
  return *digest;
}

hive::protocol::digest_type full_transaction_type::compute_sig_digest(const hive::protocol::chain_id_type& chain_id) const
{
  hive::protocol::digest_type::encoder enc;
  fc::raw::pack(enc, chain_id);
  enc.write(serialized_transaction.begin, serialized_transaction.transaction_end - serialized_transaction.begin);
  return enc.result();
}

const flat_set<hive::protocol::public_key_type>& full_transaction_type::get_signature_keys() const
{
  if (!signature_info)
  {
    ++non_cached_get_signature_keys_calls;
    fc::time_point computation_start = fc::time_point::now();
    // look up the chain_id and signature type required to validate this transaction.  If this transaction was part
    // of a block, validate based on the rules effective at the block's timestamp.  If it's a standalone transaction,
    // use the present-day rules.
    const transaction_signature_validation_rules_type* validation_rules;
    if (storage.which() == storage_type::tag<contained_in_block_info>::value)
    {
      const contained_in_block_info& contained_in_block = storage.get<contained_in_block_info>();
      assert(contained_in_block.block_storage->block);
      FC_ASSERT(contained_in_block.block_storage->block, "block should have already been decoded");
      validation_rules = &get_transaction_signature_validation_rules_at_time(contained_in_block.block_storage->block->timestamp);
    }
    else
      validation_rules = &get_signature_validation_for_new_transactions();

    signature_info_type new_signature_info;
    new_signature_info.sig_digest = compute_sig_digest(validation_rules->chain_id);

    try
    {
      try
      {
        for (const hive::protocol::signature_type& signature : get_transaction().signatures)
          HIVE_ASSERT(new_signature_info.signature_keys.insert(fc::ecc::public_key(signature, new_signature_info.sig_digest, validation_rules->signature_type)).second,
                      hive::protocol::tx_duplicate_sig,
                      "Duplicate Signature detected");
      }
      FC_RETHROW_EXCEPTIONS(error, "")
    }
    catch (const fc::exception& e)
    {
      new_signature_info.signature_keys_exception = e.dynamic_copy_exception();
    }
    new_signature_info.computation_time = fc::time_point::now() - computation_start;
    signature_info = std::move(new_signature_info);
  }
  else
  {
    ++cached_get_signature_keys_calls;
    // ilog("get_signature_keys cache hit.  saved ${saved}µs, totals: ${cached} cached, ${not} not cached", 
    //      ("saved", signature_info->computation_time)
    //      ("cached", cached_get_signature_keys_calls.load())
    //      ("not", non_cached_get_signature_keys_calls.load()));
  }
  if (signature_info->signature_keys_exception)
    signature_info->signature_keys_exception->dynamic_rethrow_exception();
  return signature_info->signature_keys;
}

void full_transaction_type::validate(std::function<void(const hive::protocol::operation& op, bool post)> notify /* = std::function<void(const operation&, bool)>() */) const
{
  if (!validation_attempted)
  {
    ++non_cached_validate_calls;
    fc::time_point computation_start = fc::time_point::now();
    try
    {
      try
      {
        if (notify)
          get_transaction().validate(notify);
        else
          get_transaction().validate();
      }
      FC_RETHROW_EXCEPTIONS(error, "")
    }
    catch (const fc::exception& e)
    {
      validation_exception = e.dynamic_copy_exception();
    }
    validation_computation_time = fc::time_point::now() - computation_start;
    validation_attempted = true;
    // ilog("validate cache miss.  cost ${cost}µs, totals: ${cached} cached, ${not} not cached", 
    //      ("cost", validation_computation_time)("cached", cached_validate_calls.load())("not", non_cached_validate_calls.load()));
    // idump((get_transaction()));
  }
  else
  {
    ++cached_validate_calls;
    // ilog("validate cache hit.  saved ${saved}µs, totals: ${cached} cached, ${not} not cached", 
    //      ("saved", validation_computation_time)("cached", cached_validate_calls.load())("not", non_cached_validate_calls.load()));
  }
  if (validation_exception)
    validation_exception->dynamic_rethrow_exception();
}

const hive::protocol::required_authorities_type& full_transaction_type::get_required_authorities() const
{
  if (!required_authorities)
  {
    ++non_cached_get_required_authorities_calls;
    auto computation_start = std::chrono::high_resolution_clock::now();
    required_authorities = hive::protocol::get_required_authorities(get_transaction().operations);
    required_authorities_computation_time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - computation_start);
  }
  else
  {
    ++cached_get_required_authorities_calls;
    // ilog("get_required_authorities cache hit.  saved ${saved}ns, totals: ${cached} cached, ${not} not cached", 
    //      ("saved", required_authorities_computation_time)
    //      ("cached", cached_get_required_authorities_calls.load())("not", non_cached_get_required_authorities_calls.load()));
  }
  return *required_authorities;
}

bool full_transaction_type::is_legacy_pack() const
{
  if (!is_packed_in_legacy_format)
  {
    hive::protocol::serialization_mode_controller::pack_guard guard(hive::protocol::pack_type::legacy);
    const hive::protocol::signed_transaction& transaction = get_transaction();
    size_t packed_size = fc::raw::pack_size(transaction);
    if (packed_size != get_transaction_size())
      is_packed_in_legacy_format = false;
    else
    {
      std::unique_ptr<char[]> packed_bytes(new char[packed_size]);
      fc::datastream<char*> stream(packed_bytes.get(), packed_size);
      fc::raw::pack(stream, transaction);
      is_packed_in_legacy_format = memcmp(packed_bytes.get(), serialized_transaction.begin, packed_size) == 0;
    }
  }
  return *is_packed_in_legacy_format;
}

size_t full_transaction_type::get_transaction_size() const
{ 
  return serialized_transaction.signed_transaction_end - serialized_transaction.begin;
}

const serialized_transaction_data& full_transaction_type::get_serialized_transaction() const
{
  return serialized_transaction;
}

const hive::protocol::transaction_id_type& full_transaction_type::get_transaction_id() const
{
  if (!transaction_id)
  {
    const hive::protocol::digest_type& digest = get_digest();
    transaction_id = hive::protocol::transaction_id_type();
    memcpy(transaction_id->_hash, digest._hash, std::min(sizeof(hive::protocol::transaction_id_type), sizeof(hive::protocol::digest_type)));
  }
  return *transaction_id;
}

/* static */ std::shared_ptr<full_transaction_type> full_transaction_type::create_from_block(const std::shared_ptr<decoded_block_storage_type>& block_storage, 
                                                                                             uint32_t index_in_block, 
                                                                                             const serialized_transaction_data& serialized_transaction,
                                                                                             bool use_transaction_cache)
{
  std::shared_ptr<full_transaction_type> full_transaction = std::make_shared<full_transaction_type>();
  full_transaction->storage = contained_in_block_info{block_storage, index_in_block};
  full_transaction->serialized_transaction = serialized_transaction;

  return use_transaction_cache ? full_transaction_cache::get_instance().add_to_cache(full_transaction) : full_transaction;
}

/* static */ std::shared_ptr<full_transaction_type> full_transaction_type::create_from_signed_transaction(const hive::protocol::signed_transaction& transaction,
                                                                                                          hive::protocol::pack_type serialization_type,
                                                                                                          bool use_transaction_cache)
{
  std::shared_ptr<full_transaction_type> full_transaction = std::make_shared<full_transaction_type>();

  full_transaction->storage.set_which(storage_type::tag<standalone_transaction_info>::value);
  standalone_transaction_info& transaction_info = full_transaction->storage.get<standalone_transaction_info>();
  transaction_info.transaction = transaction;

  hive::protocol::serialization_mode_controller::pack_guard guard(serialization_type);
  transaction_info.serialization_buffer.raw_size = fc::raw::pack_size(transaction);
  transaction_info.serialization_buffer.raw_bytes.reset(new char[transaction_info.serialization_buffer.raw_size]);
  full_transaction->serialized_transaction.begin = transaction_info.serialization_buffer.raw_bytes.get();
  fc::datastream<char*> stream(transaction_info.serialization_buffer.raw_bytes.get(), transaction_info.serialization_buffer.raw_size);
  fc::raw::pack(stream, (hive::protocol::transaction)transaction);
  full_transaction->serialized_transaction.transaction_end = full_transaction->serialized_transaction.begin + stream.tellp();
  fc::raw::pack(stream, transaction.signatures);
  full_transaction->serialized_transaction.signed_transaction_end = full_transaction->serialized_transaction.begin + stream.tellp();
  return use_transaction_cache ? full_transaction_cache::get_instance().add_to_cache(full_transaction) : full_transaction;
}

/* static */ std::shared_ptr<full_transaction_type> full_transaction_type::create_from_serialized_transaction(const char* raw_data, size_t size, bool use_transaction_cache)
{
  std::shared_ptr<full_transaction_type> full_transaction = std::make_shared<full_transaction_type>();
  full_transaction->storage.set_which(storage_type::tag<standalone_transaction_info>::value);

  // copy the serialized transaction into the full_transaction's buffer
  standalone_transaction_info& transaction_info = full_transaction->storage.get<standalone_transaction_info>();
  transaction_info.serialization_buffer.raw_size = size;
  transaction_info.serialization_buffer.raw_bytes.reset(new char[size]);
  memcpy(transaction_info.serialization_buffer.raw_bytes.get(), raw_data, size);

  // then unpack the serialized transaction
  fc::datastream<char*> datastream(transaction_info.serialization_buffer.raw_bytes.get(), transaction_info.serialization_buffer.raw_size);
  full_transaction->serialized_transaction.begin = transaction_info.serialization_buffer.raw_bytes.get();
  fc::raw::unpack(datastream, (hive::protocol::transaction&)transaction_info.transaction);
  full_transaction->serialized_transaction.transaction_end = transaction_info.serialization_buffer.raw_bytes.get() + datastream.tellp();
  fc::raw::unpack(datastream, transaction_info.transaction.signatures);
  full_transaction->serialized_transaction.signed_transaction_end = transaction_info.serialization_buffer.raw_bytes.get() + datastream.tellp();
  return use_transaction_cache ? full_transaction_cache::get_instance().add_to_cache(full_transaction) : full_transaction;
}

// tl;dr Over the lifetime of the hive blockchain, we have had three different rules for how to validate
// a transaction signature.  This function tells you what rules were in effect at a given time.
//
//  - At hardfork 0.20, we started using bip_0062 signatures.  At hardfork 
//  - 1.24, we changed the chain_id, which is used in the signature calculation.
// originally, the transaction validation code just queried the current blockchain state to find out
// which hardforks had been activated to determine what rules would be used to validate transactions,
// and this worked well.  Now, to improve performance, we'd like to start pushing off some of the 
// signature checks into another thread, starting the work as soon as we get the transactions/blocks,
// so that the expensive computation will be completed well before the transactions are included in
// the blockchain (when resyncing the blockchain, this could be 100,000+ blocks ahead of the head 
// block.  To begin advance validation of these transactions, we need to know what rules will be 
// in effect at the time they're included in the blockchain, not what rules are being followed 
// at the current head block.
//
// So when validating transactions that come in from a block, we'll call this function to find
// out what rules to use.  When validating transactions coming in from the p2p network that aren't
// part of the block, we know we must use the post hardfork 1.24 rules.
namespace
{
  transaction_signature_validation_rules_type pre_hf_0_20_rules = {OLD_CHAIN_ID, fc::ecc::fc_canonical};
  transaction_signature_validation_rules_type post_hf_0_20_rules = {OLD_CHAIN_ID, fc::ecc::bip_0062};
#ifndef USE_ALTERNATE_CHAIN_ID
  transaction_signature_validation_rules_type post_hf_1_24_rules = {HIVE_CHAIN_ID, fc::ecc::bip_0062};
#endif
}

#ifdef USE_ALTERNATE_CHAIN_ID
// on testnets, chain_id is passed on the command line, so we can't hard-code it
void set_chain_id_for_transaction_signature_validation(const chain_id_type& chain_id)
{
  pre_hf_0_20_rules.chain_id = chain_id;
  post_hf_0_20_rules.chain_id = chain_id;
}
#endif

const transaction_signature_validation_rules_type& get_transaction_signature_validation_rules_at_time(fc::time_point_sec time)
{
#ifdef USE_ALTERNATE_CHAIN_ID
  // testnet -- always uses the same chain_id, so no change of behavior at hf 1.24
  return time.sec_since_epoch() > HIVE_HARDFORK_0_20_ACTUAL_TIME ? post_hf_0_20_rules : pre_hf_0_20_rules;
#else
  // mainnet rules
  if (time.sec_since_epoch() > HIVE_HARDFORK_1_24_ACTUAL_TIME)
    return post_hf_1_24_rules;
  if (time.sec_since_epoch() > HIVE_HARDFORK_0_20_ACTUAL_TIME)
    return post_hf_0_20_rules;
  return pre_hf_0_20_rules;
#endif
}

const transaction_signature_validation_rules_type& get_signature_validation_for_new_transactions()
{
#ifdef USE_ALTERNATE_CHAIN_ID
  return post_hf_0_20_rules;
#else
  return post_hf_1_24_rules;
#endif
}

struct full_transaction_cache::impl
{
  std::mutex cache_mutex;
  typedef std::map<hive::protocol::digest_type, std::weak_ptr<full_transaction_type>> full_transaction_map_type;
  full_transaction_map_type cache;

  // in the expected use cases, the transactions will be created by the p2p thread, api thread, or other
  // helper thread.  transactions will usually be deleted by the blockchain worker thread.  instead of locking
  // to delete, we'll just queue up deletions to happen the next time a transaction is added.  that should move
  // any contention out of the blockchain worker thread
  boost::lockfree::queue<hive::protocol::digest_type> digests_to_delete{1000};

  std::atomic<uint32_t> total_lock_count = {0};
  std::atomic<uint32_t> contended_lock_count = {0};
};

full_transaction_cache::full_transaction_cache() : my(new impl) {}

std::shared_ptr<full_transaction_type> full_transaction_cache::get_by_merkle_digest(const hive::protocol::digest_type& merkle_digest)
{
  fc::optional<fc::microseconds> wait_duration;
  BOOST_SCOPE_EXIT(&my, &wait_duration) {
    my->total_lock_count.fetch_add(1, std::memory_order_relaxed);
    if (wait_duration)
    {
      my->contended_lock_count.fetch_add(1, std::memory_order_relaxed);
      wlog("full_transaction_cache lock contention, waited ${duration}µs, lock has been in use ${contended_lock_count} of ${total_lock_count} times it was used", 
           ("duration", *wait_duration)("contended_lock_count", my->contended_lock_count.load())("total_lock_count", my->total_lock_count.load()));

    }
  } BOOST_SCOPE_EXIT_END

  std::unique_lock<std::mutex> lock(my->cache_mutex, std::try_to_lock_t());
  if (!lock)
  {
    fc::time_point wait_start_time = fc::time_point::now();
    lock.lock();
    wait_duration = fc::time_point::now() - wait_start_time;
  }
  try
  {
    return my->cache.at(merkle_digest).lock();
  }
  catch (const std::out_of_range&)
  {
    return std::shared_ptr<full_transaction_type>();
  }
}

std::shared_ptr<full_transaction_type> full_transaction_cache::add_to_cache(const std::shared_ptr<full_transaction_type>& transaction)
{
  fc::optional<fc::microseconds> wait_duration;
  BOOST_SCOPE_EXIT(&my, &wait_duration) {
    my->total_lock_count.fetch_add(1, std::memory_order_relaxed);
    if (wait_duration)
    {
      my->contended_lock_count.fetch_add(1, std::memory_order_relaxed);
      wlog("full_transaction_cache lock contention, waited ${duration}µs, lock has been in use ${contended_lock_count} of ${total_lock_count} times it was used", 
           ("duration", *wait_duration)("contended_lock_count", my->contended_lock_count.load())("total_lock_count", my->total_lock_count.load()));

    }
  } BOOST_SCOPE_EXIT_END

  std::unique_lock<std::mutex> lock(my->cache_mutex, std::try_to_lock_t());
  if (!lock)
  {
    fc::time_point wait_start_time = fc::time_point::now();
    lock.lock();
    wait_duration = fc::time_point::now() - wait_start_time;
  }

  my->digests_to_delete.consume_all([&](const hive::protocol::digest_type& digest_to_delete) {
    auto iter = my->cache.find(digest_to_delete);
    if (iter != my->cache.end() && iter->second.expired())
      my->cache.erase(iter);
  });

  auto result = my->cache.insert(std::make_pair(transaction->get_merkle_digest(), transaction));
  if (result.second) // insert succeeded
  {
    transaction->set_is_in_cache();
    return transaction;
  }
  // else there was already an entry, see if it's still valid
  std::shared_ptr<full_transaction_type> existing_transaction = result.first->second.lock();
  if (existing_transaction)
    return existing_transaction;
  result.first->second = transaction;
  return transaction;
}

void full_transaction_cache::remove_from_cache(const hive::protocol::digest_type& merkle_digest)
{
  my->digests_to_delete.push(merkle_digest);
}

/* static */ full_transaction_cache& full_transaction_cache::get_instance()
{
  static full_transaction_cache the_cache;
  return the_cache;
}

} } // end namespace hive::chain
