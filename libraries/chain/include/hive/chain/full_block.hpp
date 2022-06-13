#pragma once
#include <hive/chain/block_log.hpp>
#include <hive/chain/full_transaction.hpp>
#include <hive/protocol/block.hpp>
#include <atomic>

namespace hive { namespace chain {

// stores a compressed block and the information about how it was compressed
// (the attributes are needed to correctly decompress it & to tell what peers
// can understand it)
struct compressed_block_data
{
  block_log::block_attributes_t compression_attributes;
  std::unique_ptr<char[]> compressed_bytes;
  size_t compressed_size;
};

// stores a serialized block
struct uncompressed_block_data
{
  std::unique_ptr<char[]> raw_bytes;
  size_t raw_size;
};


struct decoded_block_storage_type
{
  uncompressed_block_data uncompressed_block;
  fc::optional<signed_block> block;
};

class full_block_type
{
  private:
    mutable fc::optional<compressed_block_data> compressed_block;
    mutable std::shared_ptr<decoded_block_storage_type> decoded_block_storage;

    mutable size_t block_header_size; // only valid when block is non-null
    mutable size_t signed_block_header_size; // only valid when block is non-null

    mutable fc::optional<block_id_type> block_id;
    mutable fc::optional<digest_type> digest;
    mutable fc::optional<checksum_type> merkle_root;

    std::vector<std::shared_ptr<full_transaction_type>> full_transactions;

    static std::atomic<uint32_t> number_of_instances_created;
    static std::atomic<uint32_t> number_of_instances_destroyed;
  public:
    full_block_type();
    full_block_type(full_block_type&& rhs);
    ~full_block_type();

    static std::shared_ptr<full_block_type> create_from_compressed_block_data(std::unique_ptr<char[]>&& compressed_bytes, 
                                                                              size_t compressed_size,
                                                                              const block_log::block_attributes_t& compression_attributes);
    static std::shared_ptr<full_block_type> create_from_uncompressed_block_data(std::unique_ptr<char[]>&& raw_bytes, size_t raw_size);
    static std::shared_ptr<full_block_type> create_from_signed_block(const signed_block& block);

    static std::shared_ptr<full_block_type> create_from_block_header_and_transactions(const block_header& header, 
                                                                                      const std::vector<std::shared_ptr<full_transaction_type>>& full_transactions,
                                                                                      const fc::ecc::private_key* signer);

    void decode();

    const signed_block& get_block() const;
    const signed_block_header& get_block_header() const;
    const block_id_type& get_block_id() const;
    uint32_t get_block_num() const;
    const digest_type& get_digest() const;
    fc::ecc::public_key get_signing_key() const;
    const uncompressed_block_data& get_uncompressed_block() const;
    const compressed_block_data& get_compressed_block() const;
    uint32_t get_uncompressed_block_size() const;
    const std::vector<std::shared_ptr<full_transaction_type>>& get_full_transactions() const;
    static checksum_type compute_merkle_root(const std::vector<std::shared_ptr<full_transaction_type>>& full_transactions);
    const checksum_type& get_merkle_root() const;
};

} } // end namespace hive::chain
