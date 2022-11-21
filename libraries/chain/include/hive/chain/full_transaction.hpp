#pragma once
#include <hive/protocol/transaction.hpp>
#include <fc/reflect/reflect.hpp>
#include <hive/protocol/transaction_util.hpp>
#include <chrono>
#include <mutex>
#include <atomic>

namespace hive { namespace chain {

struct decoded_block_storage_type;

// used to store serialized (but not compressed) transactions or blocks
struct uncompressed_memory_buffer
{
  std::unique_ptr<char[]> raw_bytes;
  size_t raw_size = 0;
};

struct serialized_transaction_data
{
  const char* begin = nullptr; // pointer to the first byte of the transaction in the memory buffer
  const char* transaction_end = nullptr; // pointer past the last byte of the transaction (slice) in the memory buffer
  const char* signed_transaction_end = nullptr; // pointer past the last byte of the signed_transaction in the memory buffer
};

struct full_transaction_type;

using full_transaction_ptr = std::shared_ptr<full_transaction_type>;

struct full_transaction_type
{
  private:
    mutable std::mutex results_mutex; // single mutex used to guard writes to any data

    mutable hive::protocol::digest_type merkle_digest; // transaction hash used for calculating block's merkle root
    mutable hive::protocol::digest_type digest; // hash used for generating transaction id

    mutable fc::exception_ptr validation_exception; // if validate() threw, this is what it threw 
    mutable fc::microseconds validation_computation_time;

    struct signature_info_type
    {
      hive::protocol::digest_type sig_digest;
      flat_set<hive::protocol::public_key_type> signature_keys;
      fc::exception_ptr signature_keys_exception; // ABW: do we need separate exception? - it should be merged with validation_exception
      fc::microseconds computation_time;
    };
    mutable signature_info_type signature_info; // if we've computed the public keys that signed the transaction, it's stored here

    // ABW: it takes 128 bytes of space (plus actual account names) for 1us CPU time on average
    mutable hive::protocol::required_authorities_type required_authorities; // if we've figured out who is supposed to sign this tranaction, it's here
    mutable std::chrono::nanoseconds required_authorities_computation_time;

    /// immutable data below here isn't accessed across multiple threads, it's set at construction time and left alone
    
    // if this full_transaction was created while deserializing a block, we store
    // containing_block_info, and our signed_transaction and serialized data point
    // into data owned by the full_block
    struct contained_in_block_info
    {
      std::shared_ptr<decoded_block_storage_type> block_storage; 
      uint32_t index_in_block;
    };

    // if this full_transaction was created from a stand-alone transaction instead of from a block, we 
    // store the data in this structure
    struct standalone_transaction_info
    {
      std::unique_ptr<hive::protocol::signed_transaction> transaction;
      uncompressed_memory_buffer serialization_buffer;
    };
    typedef std::variant<contained_in_block_info, standalone_transaction_info> storage_type;
    storage_type storage;

    serialized_transaction_data serialized_transaction; // pointers to the beginning, middle, and end of the transaction in the storage
    mutable int64_t rc_cost = -1; // RC cost of transaction - set when transaction is processed first, can be overwritten when it becomes part of block

    mutable fc::ripemd160 legacy_transaction_message_hash; // hash of p2p transaction message generated from this transaction
    mutable hive::protocol::transaction_id_type transaction_id; // transaction id itself (truncated digest)

    mutable bool is_packed_in_legacy_format = false;
    mutable bool is_in_cache = false; // true if this is tracked in the global transaction cache; if so, we need to remove ourselves upon garbage collection

    mutable std::atomic<bool> has_merkle_digest = { false };
    mutable std::atomic<bool> has_legacy_transaction_message_hash = { false };
    mutable std::atomic<bool> has_digest_and_transaction_id = { false };
    mutable std::atomic<bool> validation_attempted = { false }; // true if validate() has been called & cached
    mutable std::atomic<bool> validation_accessed = false;
    mutable std::atomic<bool> has_is_packed_in_legacy_format = { false };
    mutable std::atomic<bool> has_signature_info = { false };
    mutable std::atomic<bool> signature_keys_accessed = { false };
    mutable std::atomic<bool> has_required_authorities = { false };
    mutable std::atomic<bool> required_authorities_accessed = { false };


    static std::atomic<uint32_t> number_of_instances_created;
    static std::atomic<uint32_t> number_of_instances_destroyed;
    
    /// Helper method encapsulating full_transaction object creation.
    static full_transaction_ptr build_transaction_object(const hive::protocol::signed_transaction& transaction, hive::protocol::pack_type serialization_type);
    static serialized_transaction_data fill_serialization_buffer(const hive::protocol::signed_transaction& transaction, hive::protocol::pack_type serialization_type,
      uncompressed_memory_buffer* serialization_buffer);

  public:
    full_transaction_type();
    ~full_transaction_type();

    void  set_is_in_cache() { is_in_cache = true; }
    const hive::protocol::signed_transaction& get_transaction() const;
    const hive::protocol::digest_type& get_merkle_digest() const;
    const hive::protocol::digest_type& get_digest() const;
    hive::protocol::digest_type compute_sig_digest(const hive::protocol::chain_id_type& chain_id) const;
    const fc::ripemd160& get_legacy_transaction_message_hash() const;
    const hive::protocol::transaction_id_type& get_transaction_id() const;
    void compute_signature_keys() const;
    const flat_set<hive::protocol::public_key_type>& get_signature_keys() const;
    void compute_required_authorities() const;
    const hive::protocol::required_authorities_type& get_required_authorities() const;
    bool is_legacy_pack() const;
    void precompute_validation(std::function<void(const hive::protocol::operation& op, bool post)> notify = std::function<void(const hive::protocol::operation&, bool)>()) const;
    void validate(std::function<void(const hive::protocol::operation& op, bool post)> notify = std::function<void(const hive::protocol::operation&, bool)>()) const;
    void set_rc_cost( int64_t cost ) const { rc_cost = cost; } // can only be called under main lock of write thread

    const serialized_transaction_data& get_serialized_transaction() const;
    size_t get_transaction_size() const;
    int64_t get_rc_cost() const { return rc_cost; }

    template <class DataStream>
    void dump_serialized_transaction(DataStream& datastream) const
    {
      const serialized_transaction_data& serialized_transaction = get_serialized_transaction();
      datastream.write(serialized_transaction.begin, serialized_transaction.signed_transaction_end - serialized_transaction.begin);
    }

    /// Allows to sign transaction and append signature to the underlying signed_transaction::signatures container;
    void sign_transaction(const std::vector<hive::protocol::private_key_type>& keys, const hive::protocol::chain_id_type& chain_id,
      fc::ecc::canonical_signature_type canon_type, hive::protocol::pack_type serialization_type);

    static full_transaction_ptr create_from_block(const std::shared_ptr<decoded_block_storage_type>& block_storage, uint32_t index_in_block,
                                                  const serialized_transaction_data& serialized_transaction, bool use_transaction_cache);
    /// Allows to build a full_transaction object basing on not yet signed transaction 
    static full_transaction_ptr create_from_transaction(const hive::protocol::transaction& transaction, hive::protocol::pack_type serialization_type);
    /// Allows to build a full_transaction object from ALREADY signed transaction (pointed transaction object must contain at least one signature).
    static full_transaction_ptr create_from_signed_transaction(const hive::protocol::signed_transaction& transaction,
                                                               hive::protocol::pack_type serialization_type, bool use_transaction_cache);
    static full_transaction_ptr create_from_serialized_transaction(const char* raw_data, size_t size, bool use_transaction_cache);
};

class full_transaction_cache
{
  struct impl;
  std::unique_ptr<impl> my;
  full_transaction_cache();
public:
  full_transaction_ptr add_to_cache(const full_transaction_ptr& transaction);
  void remove_from_cache(const hive::protocol::digest_type& merkle_digest);

  static full_transaction_cache& get_instance();
};

// utility functions to get the transaction signature validation rules for a given block
struct transaction_signature_validation_rules_type
{
  hive::protocol::chain_id_type chain_id;
  hive::protocol::canonical_signature_type signature_type;
};

#ifdef USE_ALTERNATE_CHAIN_ID
void set_chain_id_for_transaction_signature_validation(const hive::protocol::chain_id_type& chain_id);
#endif
const transaction_signature_validation_rules_type& get_transaction_signature_validation_rules_at_time(fc::time_point_sec time);
const transaction_signature_validation_rules_type& get_signature_validation_for_new_transactions();

} } // end namespace hive::chain

FC_REFLECT_TYPENAME(hive::chain::full_transaction_type::contained_in_block_info)
FC_REFLECT_TYPENAME(hive::chain::full_transaction_type::standalone_transaction_info)
