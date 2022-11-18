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

  if (validation_attempted.load(std::memory_order_relaxed) && !validation_accessed.load(std::memory_order_relaxed))
    fc_ilog(fc::logger::get("worker_thread"), "transaction validation pre-computed, but full_transaction_type::validate() was never called");
  if (has_signature_info.load(std::memory_order_relaxed) && !signature_keys_accessed.load(std::memory_order_relaxed))
    fc_ilog(fc::logger::get("worker_thread"), "transaction signature keys pre-computed, but full_transaction_type::get_signature_keys() was never called");
  if (has_required_authorities.load(std::memory_order_relaxed) && !required_authorities_accessed.load(std::memory_order_relaxed))
    fc_ilog(fc::logger::get("worker_thread"), "transaction required authorities pre-computed, but full_transaction_type::get_required_authorities() was never called");
}

const signed_transaction& full_transaction_type::get_transaction() const
{ 
  if (std::holds_alternative<contained_in_block_info>(storage))
  {
    const contained_in_block_info& contained_in_block = std::get<contained_in_block_info>(storage);
    assert(contained_in_block.block_storage->block);
    FC_ASSERT(contained_in_block.block_storage->block, "block should have already been decoded");
    return contained_in_block.block_storage->block->transactions[contained_in_block.index_in_block];
  }
  else //this is a standalone transaction
  {
    const standalone_transaction_info& standalone_transaction = std::get<standalone_transaction_info>(storage);
    return *standalone_transaction.transaction;
  }
}

const hive::protocol::digest_type& full_transaction_type::get_merkle_digest() const
{
  if (!has_merkle_digest.load(std::memory_order_consume))
  {
    std::lock_guard<std::mutex> guard(results_mutex);
    if (!has_merkle_digest.load(std::memory_order_consume))
    {
      merkle_digest = hive::protocol::digest_type::hash(serialized_transaction.begin, serialized_transaction.signed_transaction_end - serialized_transaction.begin);
      has_merkle_digest.store(true, std::memory_order_release);
    }
  }
  return merkle_digest;
}

const fc::ripemd160& full_transaction_type::get_legacy_transaction_message_hash() const
{
  if (!has_legacy_transaction_message_hash.load(std::memory_order_consume))
  {
    std::lock_guard<std::mutex> guard(results_mutex);
    if (!has_legacy_transaction_message_hash.load(std::memory_order_consume))
    {
      legacy_transaction_message_hash = fc::ripemd160::hash(serialized_transaction.begin, serialized_transaction.signed_transaction_end - serialized_transaction.begin);
      has_legacy_transaction_message_hash.store(true, std::memory_order_release);
    }
  }
  return legacy_transaction_message_hash;
}

const hive::protocol::digest_type& full_transaction_type::get_digest() const
{
  if (!has_digest_and_transaction_id.load(std::memory_order_consume))
  {
    std::lock_guard<std::mutex> guard(results_mutex);
    if (!has_digest_and_transaction_id.load(std::memory_order_consume))
    {
      digest = hive::protocol::digest_type::hash(serialized_transaction.begin, serialized_transaction.transaction_end - serialized_transaction.begin);
      transaction_id = hive::protocol::transaction_id_type();
      memcpy(transaction_id._hash, digest._hash, std::min(sizeof(hive::protocol::transaction_id_type), sizeof(hive::protocol::digest_type)));
      has_digest_and_transaction_id.store(true, std::memory_order_release);
    }
  }

  return digest;
}

hive::protocol::digest_type full_transaction_type::compute_sig_digest(const hive::protocol::chain_id_type& chain_id) const
{
  hive::protocol::digest_type::encoder enc;
  fc::raw::pack(enc, chain_id);
  enc.write(serialized_transaction.begin, serialized_transaction.transaction_end - serialized_transaction.begin);
  return enc.result();
}

void full_transaction_type::compute_signature_keys() const
{
  std::lock_guard<std::mutex> guard(results_mutex);
  if (!has_signature_info.load(std::memory_order_consume))
  {
    fc::time_point computation_start = fc::time_point::now();
    // look up the chain_id and signature type required to validate this transaction.  If this transaction was part
    // of a block, validate based on the rules effective at the block's timestamp.  If it's a standalone transaction,
    // use the present-day rules.
    const transaction_signature_validation_rules_type* validation_rules;
    if (std::holds_alternative<contained_in_block_info>(storage))
    {
      const contained_in_block_info& contained_in_block = std::get<contained_in_block_info>(storage);
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

    has_signature_info.store(true, std::memory_order_release);
  }
}

const flat_set<hive::protocol::public_key_type>& full_transaction_type::get_signature_keys() const
{
  if (!has_signature_info.load(std::memory_order_consume))
  {
    compute_signature_keys();
    cached_get_signature_keys_calls.fetch_add(1, std::memory_order_relaxed);
  }
  else
  {
    cached_get_signature_keys_calls.fetch_add(1, std::memory_order_relaxed);
    // ilog("get_signature_keys cache hit.  saved ${saved}µs, totals: ${cached} cached, ${not} not cached", 
    //      ("saved", signature_info->computation_time)
    //      ("cached", cached_get_signature_keys_calls.load())
    //      ("not", non_cached_get_signature_keys_calls.load()));
  }

  signature_keys_accessed.store(true, std::memory_order_relaxed);

  if (signature_info.signature_keys_exception)
    signature_info.signature_keys_exception->dynamic_rethrow_exception();
  return signature_info.signature_keys;
}

void full_transaction_type::precompute_validation(std::function<void(const hive::protocol::operation& op, bool post)> notify /* = std::function<void(const operation&, bool)>() */) const
{
  std::lock_guard<std::mutex> results_guard(results_mutex);
  if (!validation_attempted.load(std::memory_order_consume))
  {
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
    validation_attempted.store(true, std::memory_order_release);
    // ilog("validate cache miss.  cost ${cost}µs, totals: ${cached} cached, ${not} not cached", 
    //      ("cost", validation_computation_time)("cached", cached_validate_calls.load())("not", non_cached_validate_calls.load()));
    // idump((get_transaction()));
  }
}

void full_transaction_type::validate(std::function<void(const hive::protocol::operation& op, bool post)> notify /* = std::function<void(const operation&, bool)>() */) const
{
  if (!validation_attempted.load(std::memory_order_consume))
  {
    precompute_validation(notify);
    non_cached_validate_calls.fetch_add(1, std::memory_order_relaxed);
  }
  else
  {
    cached_validate_calls.fetch_add(1, std::memory_order_relaxed);
    // ilog("validate cache hit.  saved ${saved}µs, totals: ${cached} cached, ${not} not cached", 
    //      ("saved", validation_computation_time)("cached", cached_validate_calls.load())("not", non_cached_validate_calls.load()));
  }
  validation_accessed.store(true, std::memory_order_relaxed);
  if (validation_exception)
    validation_exception->dynamic_rethrow_exception();
}

void full_transaction_type::compute_required_authorities() const
{
  std::lock_guard<std::mutex> guard(results_mutex);
  if (!has_required_authorities.load(std::memory_order_consume))
  {
    auto computation_start = std::chrono::high_resolution_clock::now();
    required_authorities = hive::protocol::get_required_authorities(get_transaction().operations);
    required_authorities_computation_time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - computation_start);

    has_required_authorities.store(true, std::memory_order_release);
  }
}
const hive::protocol::required_authorities_type& full_transaction_type::get_required_authorities() const
{
  if (!has_required_authorities.load(std::memory_order_consume))
  {
    compute_required_authorities();
    non_cached_get_required_authorities_calls.fetch_add(1, std::memory_order_relaxed);
  }
  else
  {
    cached_get_required_authorities_calls.fetch_add(1, std::memory_order_relaxed);
    // ilog("get_required_authorities cache hit.  saved ${saved}ns, totals: ${cached} cached, ${not} not cached", 
    //      ("saved", required_authorities_computation_time)
    //      ("cached", cached_get_required_authorities_calls.load())("not", non_cached_get_required_authorities_calls.load()));
  }
  required_authorities_accessed.store(true, std::memory_order_relaxed);
  return required_authorities;
}

bool full_transaction_type::is_legacy_pack() const
{
  if (!has_is_packed_in_legacy_format.load(std::memory_order_consume))
  {
    std::lock_guard<std::mutex> results_guard(results_mutex);

    if (!has_is_packed_in_legacy_format.load(std::memory_order_consume))
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

      has_is_packed_in_legacy_format.store(true, std::memory_order_release);
    }
  }
  return is_packed_in_legacy_format;
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
  if (!has_digest_and_transaction_id.load(std::memory_order_consume))
    (void)get_digest(); // guaranteed to compute transaction_id as a side effect
  return transaction_id;
}

void full_transaction_type::sign_transaction(const std::vector<hive::protocol::private_key_type>& keys, const hive::protocol::chain_id_type& chain_id,
  fc::ecc::canonical_signature_type canon_type, hive::protocol::pack_type serialization_type)
{
  FC_ASSERT(std::holds_alternative<standalone_transaction_info>(storage), "Only standalone transactions can be signed");

  standalone_transaction_info& standalone_transaction = std::get<standalone_transaction_info>(storage);
  signed_transaction& tx = *standalone_transaction.transaction;

  /// Update serialization buffer to reflect requested serialization_type
  serialized_transaction = fill_serialization_buffer(tx, serialization_type, &standalone_transaction.serialization_buffer);

  auto sig_digest = compute_sig_digest(chain_id);

  for(const auto& key : keys)
  {
    auto signature = key.sign_compact(sig_digest, canon_type);

    tx.signatures.emplace_back(signature);
  }

  /// Now serialized buffer must be updated, since new signatures has been added.
  serialized_transaction = fill_serialization_buffer(tx, serialization_type, &standalone_transaction.serialization_buffer);
}

/* static */ full_transaction_ptr full_transaction_type::create_from_block(const std::shared_ptr<decoded_block_storage_type>& block_storage, 
                                                                           uint32_t index_in_block, 
                                                                           const serialized_transaction_data& serialized_transaction,
                                                                           bool use_transaction_cache)
{
  full_transaction_ptr full_transaction = std::make_shared<full_transaction_type>();
  full_transaction->storage = contained_in_block_info{block_storage, index_in_block};
  full_transaction->serialized_transaction = serialized_transaction;

  return use_transaction_cache ? full_transaction_cache::get_instance().add_to_cache(full_transaction) : full_transaction;
}

/*static*/full_transaction_ptr full_transaction_type::build_transaction_object(const hive::protocol::signed_transaction& transaction, hive::protocol::pack_type serialization_type)
{
  full_transaction_ptr full_transaction = std::make_shared<full_transaction_type>();

  full_transaction->storage = standalone_transaction_info();
  standalone_transaction_info& transaction_info = std::get<standalone_transaction_info>(full_transaction->storage);
  transaction_info.transaction = std::make_unique<hive::protocol::signed_transaction>(transaction);

  full_transaction->serialized_transaction = fill_serialization_buffer(transaction, serialization_type, &transaction_info.serialization_buffer);

  return full_transaction;
}

/*static*/ serialized_transaction_data full_transaction_type::fill_serialization_buffer(const hive::protocol::signed_transaction& transaction,
  hive::protocol::pack_type serialization_type, uncompressed_memory_buffer* serialization_buffer)
{
  hive::protocol::serialization_mode_controller::pack_guard guard(serialization_type);

  serialization_buffer->raw_size = fc::raw::pack_size(transaction);
  serialization_buffer->raw_bytes.reset(new char[serialization_buffer->raw_size]);

  serialized_transaction_data data;

  data.begin = serialization_buffer->raw_bytes.get();
  fc::datastream<char*> stream(serialization_buffer->raw_bytes.get(), serialization_buffer->raw_size);
  fc::raw::pack(stream, static_cast<const hive::protocol::transaction&>(transaction));
  data.transaction_end = data.begin + stream.tellp();
  fc::raw::pack(stream, transaction.signatures);
  data.signed_transaction_end = data.begin + stream.tellp();

  return data;
}

/*static*/ full_transaction_ptr full_transaction_type::create_from_transaction(const hive::protocol::transaction& transaction,
                                                                               hive::protocol::pack_type serialization_type)
{
  signed_transaction copy(transaction);
  return build_transaction_object(copy, serialization_type);
}


/* static */ full_transaction_ptr full_transaction_type::create_from_signed_transaction(const hive::protocol::signed_transaction& transaction,
                                                                                        hive::protocol::pack_type serialization_type,
                                                                                        bool use_transaction_cache)
{
  //FC_ASSERT(transaction.signatures.empty() == false, "Passed signed transaction does not contain any signatures");

  auto full_transaction = build_transaction_object(transaction, serialization_type);

  return use_transaction_cache ? full_transaction_cache::get_instance().add_to_cache(full_transaction) : full_transaction;
}

/* static */ full_transaction_ptr full_transaction_type::create_from_serialized_transaction(const char* raw_data, size_t size, bool use_transaction_cache)
{
  full_transaction_ptr full_transaction = std::make_shared<full_transaction_type>();
  full_transaction->storage = standalone_transaction_info();

  // copy the serialized transaction into the full_transaction's buffer
  standalone_transaction_info& transaction_info = std::get<standalone_transaction_info>(full_transaction->storage);
  transaction_info.transaction = std::make_unique<hive::protocol::signed_transaction>();
  transaction_info.serialization_buffer.raw_size = size;
  transaction_info.serialization_buffer.raw_bytes.reset(new char[size]);
  memcpy(transaction_info.serialization_buffer.raw_bytes.get(), raw_data, size);

  // then unpack the serialized transaction
  fc::datastream<char*> datastream(transaction_info.serialization_buffer.raw_bytes.get(), transaction_info.serialization_buffer.raw_size);
  full_transaction->serialized_transaction.begin = transaction_info.serialization_buffer.raw_bytes.get();
  fc::raw::unpack(datastream, *(hive::protocol::transaction*)transaction_info.transaction.get());
  full_transaction->serialized_transaction.transaction_end = transaction_info.serialization_buffer.raw_bytes.get() + datastream.tellp();
  fc::raw::unpack(datastream, transaction_info.transaction->signatures);
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
#ifdef IS_TEST_NET
  // testnet -- we can't rely on time because in unit tests it will be close to genesis, but also hardforks are activated at different times manually
  return post_hf_0_20_rules; //this is bad but better than rules for mirrornet
#elif USE_ALTERNATE_CHAIN_ID
  // mirrornet -- always uses the same chain_id, so no change of behavior at hf 1.24
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

full_transaction_ptr full_transaction_cache::add_to_cache(const full_transaction_ptr& transaction)
{
  std::optional<fc::microseconds> wait_duration;
  BOOST_SCOPE_EXIT(&my, &wait_duration) {
    my->total_lock_count.fetch_add(1, std::memory_order_relaxed);
    if (wait_duration)
    {
      my->contended_lock_count.fetch_add(1, std::memory_order_relaxed);
      // TODO: disable this warning before release, this is expected right at the transition between sync & live mode
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
  full_transaction_ptr existing_transaction = result.first->second.lock();
  if (existing_transaction)
    return existing_transaction;
  result.first->second = transaction;
  transaction->set_is_in_cache();
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
