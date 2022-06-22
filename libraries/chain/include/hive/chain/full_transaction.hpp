#pragma once
#include <hive/protocol/transaction.hpp>
#include <fc/reflect/reflect.hpp>
#include <hive/protocol/transaction_util.hpp>

namespace hive { namespace chain {

struct decoded_block_storage_type;

// used to store serialized (but not compressed) transactions or blocks
struct uncompressed_memory_buffer
{
  std::unique_ptr<char[]> raw_bytes;
  size_t raw_size;
};

struct serialized_transaction_data
{
  const char* begin; // pointer to the first byte of the transaction in the memory buffer
  const char* transaction_end; // pointer past the last byte of the transaction (slice) in the memory buffer
  const char* signed_transaction_end; // pointer past the last byte of the signed_transaction in the memory buffer
};

struct full_transaction_type
{
  private:
    mutable fc::optional<hive::protocol::digest_type> merkle_digest; // transaction hash used for calculating block's merkle root
    mutable fc::optional<hive::protocol::digest_type> digest; // hash used for generating transaction id
    mutable fc::optional<hive::protocol::transaction_id_type> transaction_id; // transaction id itself (truncated digest)
    mutable bool validation_attempted = false; // true if validate() has been called & cached
    mutable fc::exception_ptr validation_exception; // if validate() threw, this is what it threw 
    mutable fc::optional<bool> is_packed_in_legacy_format;

    struct signature_info_type
    {
      hive::protocol::digest_type sig_digest;
      flat_set<hive::protocol::public_key_type> signature_keys;
      std::shared_ptr<fc::exception> signature_keys_exception;
    };
    mutable fc::optional<signature_info_type> signature_info; // if we've computed the public keys that signed the transaction, it's stored here

    mutable fc::optional<hive::protocol::required_authorities_type> required_authorities; // if we've figured out who is supposed to sign this tranaction, it's here


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
      hive::protocol::signed_transaction transaction;
      uncompressed_memory_buffer serialization_buffer;
    };
    typedef fc::static_variant<contained_in_block_info, standalone_transaction_info> storage_type;
    storage_type storage;

    serialized_transaction_data serialized_transaction; // pointers to the beginning, middle, and end of the transaction in the storage

    hive::protocol::digest_type compute_sig_digest(const hive::protocol::chain_id_type& chain_id) const;
  public:
    const hive::protocol::signed_transaction& get_transaction() const;
    const hive::protocol::digest_type& get_merkle_digest() const;
    const hive::protocol::digest_type& get_digest() const;
    const hive::protocol::transaction_id_type& get_transaction_id() const;
    const flat_set<hive::protocol::public_key_type>& get_signature_keys() const;
    const hive::protocol::required_authorities_type& get_required_authorities() const;
    bool is_legacy_pack() const;
    void validate(std::function<void(const hive::protocol::operation& op, bool post)> notify = std::function<void(const hive::protocol::operation&, bool)>()) const;

    const serialized_transaction_data& get_serialized_transaction() const;
    size_t get_transaction_size() const;

    static std::shared_ptr<full_transaction_type> create_from_block(const std::shared_ptr<decoded_block_storage_type>& block_storage,
                                                                    uint32_t index_in_block, 
                                                                    const serialized_transaction_data& serialized_transaction);
    static std::shared_ptr<full_transaction_type> create_from_signed_transaction(const hive::protocol::signed_transaction& transaction, 
                                                                                 hive::protocol::pack_type serialization_type);
    static std::shared_ptr<full_transaction_type> create_from_serialized_transaction(const char* raw_data, size_t size);
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
