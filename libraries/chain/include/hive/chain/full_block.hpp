#pragma once
#include <hive/chain/block_log.hpp>
#include <hive/chain/full_transaction.hpp>
#include <hive/protocol/block.hpp>
#include <atomic>
#include <mutex>

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
  std::optional<signed_block> block;

  decoded_block_storage_type();
  ~decoded_block_storage_type();
  static std::atomic<uint32_t> number_of_instances_created;
  static std::atomic<uint32_t> number_of_instances_destroyed;
};

class full_block_type
{
  private:
    // compressed version of the block
    // we can hold two versions of the compressed block -- the preferred version, which
    // we will store in the block log and share with capable peers; and an alternate
    // version which is compresed without a dictionary, that's capable of being shared
    // with peers that don't have the same compression dictionaries as we do
    mutable std::atomic<bool> has_compressed_block = { false };
    mutable std::atomic<bool> has_alternate_compressed_block = { false };
    mutable std::mutex compressed_block_mutex;
    mutable compressed_block_data compressed_block;
    mutable compressed_block_data alternate_compressed_block;
    mutable fc::microseconds compression_time;
    mutable fc::microseconds alternate_compression_time;

    // uncompressed version of the block
    mutable std::atomic<bool> has_uncompressed_block = { false };
    mutable std::mutex uncompressed_block_mutex;
    mutable std::shared_ptr<decoded_block_storage_type> decoded_block_storage;
    mutable fc::microseconds uncompression_time;

    mutable std::atomic<bool> has_unpacked_block_header = { false };
    mutable std::mutex unpacked_block_header_mutex;
    mutable size_t block_header_size; // only valid when has_unpacked_block_header
    mutable size_t signed_block_header_size; // only valid when has_unpacked_block_header
    mutable digest_type digest; // only valid when has_unpacked_block_header
    mutable fc::microseconds decode_block_header_time; // only valid when has_unpacked_block_header

    mutable std::atomic<bool> has_block_id = { false };
    mutable block_id_type block_id; // only valid when has_block_id


    mutable std::atomic<bool> has_legacy_block_message_hash = { false };
    mutable std::mutex legacy_block_message_hash_mutex;
    mutable fc::ripemd160 legacy_block_message_hash; // only valid when has_legacy_block_message_hash
    mutable fc::microseconds compute_legacy_block_message_hash_time; // only valid when has_legacy_block_message_hash


    mutable std::atomic<bool> has_unpacked_block = { false };
    mutable std::mutex unpacked_block_mutex;
    mutable std::vector<std::shared_ptr<full_transaction_type>> full_transactions; // only valid when has_unpacked_block
    mutable fc::microseconds decode_block_time; // only valid when has_unpacked_block

    mutable std::mutex block_signing_key_merkle_root_mutex;
    mutable std::optional<fc::ecc::public_key> block_signing_key;
    mutable bool block_signing_key_accessed = false;
    mutable std::optional<checksum_type> merkle_root;
    mutable bool merkle_key_accessed = false;
    mutable fc::microseconds compute_merkle_root_time;
    mutable fc::microseconds compute_block_signing_key_time;

    static std::atomic<uint32_t> number_of_instances_created;
    static std::atomic<uint32_t> number_of_instances_destroyed;

    static block_id_type construct_block_id(const char* signed_block_header_begin, size_t signed_block_header_size, uint32_t block_num);
  public:
    full_block_type();
    ~full_block_type();

    static std::shared_ptr<full_block_type> create_from_compressed_block_data(std::unique_ptr<char[]>&& compressed_bytes, 
                                                                              size_t compressed_size,
                                                                              const block_log::block_attributes_t& compression_attributes,
                                                                              const std::optional<block_id_type> block_id = std::optional<block_id_type>());
    static std::shared_ptr<full_block_type> create_from_uncompressed_block_data(std::unique_ptr<char[]>&& raw_bytes, size_t raw_size,
                                                                                const std::optional<block_id_type> block_id = std::optional<block_id_type>());
    static std::shared_ptr<full_block_type> create_from_signed_block(const signed_block& block);

    static std::shared_ptr<full_block_type> create_from_block_header_and_transactions(const block_header& header, 
                                                                                      const std::vector<std::shared_ptr<full_transaction_type>>& full_transactions,
                                                                                      const fc::ecc::private_key* signer);

    void decode_block() const; // immediately decompresses & unpacks the block, called by the worker thread
    const signed_block& get_block() const;
    void decode_block_header() const;
    const signed_block_header& get_block_header() const;
    const block_id_type& get_block_id() const;
    uint32_t get_block_num() const;
    bool has_decoded_block_id() const;

    void compute_legacy_block_message_hash() const;
    const fc::ripemd160& get_legacy_block_message_hash() const;
    void compute_signing_key() const;
    const fc::ecc::public_key& get_signing_key() const;

    void decompress_block() const;
    const uncompressed_block_data& get_uncompressed_block() const;
    uint32_t get_uncompressed_block_size() const;

    bool has_compressed_block_data() const;
    std::optional<uint8_t> get_best_available_zstd_compression_dictionary_number() const;
    void compress_block() const; // immediately compresses the block, called by the worker thread
    const compressed_block_data& get_compressed_block() const; // returns the compressed block (if not already compressed, this compresses the block first)
    void alternate_compress_block() const;
    const compressed_block_data& get_alternate_compressed_block() const;

    const std::vector<std::shared_ptr<full_transaction_type>>& get_full_transactions() const;
    static checksum_type compute_merkle_root(const std::vector<std::shared_ptr<full_transaction_type>>& full_transactions);
    void compute_merkle_root() const;
    const checksum_type& get_merkle_root() const;
};

} } // end namespace hive::chain
