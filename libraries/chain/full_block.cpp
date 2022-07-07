#include <hive/chain/full_block.hpp>
#include <hive/chain/block_compression_dictionaries.hpp>
#include <fc/bitutil.hpp>

#include <fc/io/json.hpp>

namespace hive { namespace chain {

/* static */ std::atomic<uint32_t> full_block_type::number_of_instances_created = {0};
/* static */ std::atomic<uint32_t> full_block_type::number_of_instances_destroyed = {0};

/* static */ std::atomic<uint32_t> decoded_block_storage_type::number_of_instances_created = {0};
/* static */ std::atomic<uint32_t> decoded_block_storage_type::number_of_instances_destroyed = {0};

decoded_block_storage_type::decoded_block_storage_type()
{
  number_of_instances_created.fetch_add(1, std::memory_order_relaxed);
  // if (number_of_instances_created.load() % 10000 == 0)
  //   ilog("Currently ${count} decoded_block_storage_types in memory", ("count", number_of_instances_created.load() - number_of_instances_destroyed.load()));
}

decoded_block_storage_type::~decoded_block_storage_type()
{
  number_of_instances_destroyed.fetch_add(1, std::memory_order_relaxed);
}

full_block_type::full_block_type()
{
  number_of_instances_created.fetch_add(1, std::memory_order_relaxed);
  if (number_of_instances_created.load() % 10000 == 0)
    ilog("Currently ${count} full_blocks in memory", ("count", number_of_instances_created.load() - number_of_instances_destroyed.load()));
}

full_block_type::~full_block_type()
{
  number_of_instances_destroyed.fetch_add(1, std::memory_order_relaxed);
}

/* static */ std::shared_ptr<full_block_type> full_block_type::create_from_compressed_block_data(std::unique_ptr<char[]>&& compressed_bytes, 
                                                                                                 size_t compressed_size,
                                                                                                 const block_log::block_attributes_t& compression_attributes)
{
  std::shared_ptr<full_block_type> full_block = std::make_shared<full_block_type>();
  full_block->compressed_block.compression_attributes = compression_attributes;
  full_block->compressed_block.compressed_bytes = std::move(compressed_bytes);
  full_block->compressed_block.compressed_size = compressed_size;
  full_block->has_compressed_block.store(true, std::memory_order_release);

  return full_block;
}

/* static */ std::shared_ptr<full_block_type> full_block_type::create_from_uncompressed_block_data(std::unique_ptr<char[]>&& raw_bytes, size_t raw_size)
{ try {
  std::shared_ptr<full_block_type> full_block = std::make_shared<full_block_type>();

  full_block->decoded_block_storage = std::make_shared<decoded_block_storage_type>();
  full_block->decoded_block_storage->uncompressed_block = uncompressed_block_data{std::move(raw_bytes), raw_size};
  full_block->has_uncompressed_block.store(true, std::memory_order_release);

  return full_block;
} FC_CAPTURE_AND_RETHROW() }

// temporary helper to construct a full_block from a signed_block, to be deleted 
/* static */ std::shared_ptr<full_block_type> full_block_type::create_from_signed_block(const signed_block& block)
{ try {
  // allocate enough storage to hold the block in its packed form
  std::shared_ptr<decoded_block_storage_type> decoded_block_storage = std::make_shared<decoded_block_storage_type>();
  decoded_block_storage->uncompressed_block.raw_size = fc::raw::pack_size(block);
  decoded_block_storage->uncompressed_block.raw_bytes.reset(new char[decoded_block_storage->uncompressed_block.raw_size]);

  // pack the block
  fc::datastream<char*> stream(decoded_block_storage->uncompressed_block.raw_bytes.get(), decoded_block_storage->uncompressed_block.raw_size);
  std::shared_ptr<full_block_type> full_block = std::make_shared<full_block_type>();
  full_block->decoded_block_storage = std::move(decoded_block_storage);
  fc::raw::pack(stream, block);
  full_block->has_uncompressed_block.store(true, std::memory_order_release);

  return full_block;
} FC_CAPTURE_AND_RETHROW() }

/* static */ std::shared_ptr<full_block_type> full_block_type::create_from_block_header_and_transactions(const block_header& header, 
                                                                                                         const std::vector<std::shared_ptr<full_transaction_type>>& full_transactions,
                                                                                                         const fc::ecc::private_key* signer)
{ try {
  std::shared_ptr<full_block_type> full_block = std::make_shared<full_block_type>();

  // start constructing the full block.  The header should be completely filled out, except
  // for the merkle root.  Copy the header over and generate the merkle_root
  full_block->decoded_block_storage = std::make_shared<decoded_block_storage_type>();
  const std::shared_ptr<decoded_block_storage_type>& decoded_block_storage = full_block->decoded_block_storage; // alias

  decoded_block_storage->block = signed_block();
  signed_block& new_block = *decoded_block_storage->block; // alias to keep things shorter
  (block_header&)new_block = header;
  full_block->merkle_root = compute_merkle_root(full_transactions);
  new_block.transaction_merkle_root = *full_block->merkle_root;

  // let's start generating the serialized version of the block:
  // first, compute the total size of all the serialized transactions
  // the signed vector will be packed as a number of transactions, followed by the
  // serialized signed_transactions
  fc::unsigned_int number_of_transactions(full_transactions.size());
  size_t total_transaction_size = fc::raw::pack_size(number_of_transactions);
  for (const std::shared_ptr<full_transaction_type>& full_transaction : full_transactions)
    total_transaction_size += full_transaction->get_transaction_size();

  // add to that the size of the header to get the size of the block as a whole
  size_t signed_header_size = fc::raw::pack_size((signed_block_header)new_block);
  size_t total_serialized_size = signed_header_size + total_transaction_size;

  // allocate space for the serialized block
  decoded_block_storage->uncompressed_block.raw_bytes.reset(new char[total_serialized_size]);
  decoded_block_storage->uncompressed_block.raw_size = total_serialized_size;
  // serialize the block.  Like when deserializing it, we do it piece by piece so we can record
  // the sizes of individual pieces
  fc::datastream<char*> datastream(decoded_block_storage->uncompressed_block.raw_bytes.get(), total_serialized_size);
  fc::raw::pack(datastream, (block_header)new_block);
  full_block->block_header_size = datastream.tellp();
  full_block->digest = digest_type::hash(decoded_block_storage->uncompressed_block.raw_bytes.get(), full_block->block_header_size);

  // now that we've serialized the data used in computing the digest, use that to sign the block
  if (signer)
  {
    const transaction_signature_validation_rules_type& validation_rules = get_transaction_signature_validation_rules_at_time(header.timestamp);
    new_block.witness_signature = signer->sign_compact(*full_block->digest, validation_rules.signature_type);
    full_block->block_signing_key = signer->get_public_key();
  }
  // and serialize the signature
  fc::raw::pack(datastream, new_block.witness_signature);
  full_block->signed_block_header_size = datastream.tellp();
  full_block->block_id = construct_block_id(decoded_block_storage->uncompressed_block.raw_bytes.get(), full_block->signed_block_header_size, header.block_num());

  // then serialize the vector of transactions
  fc::raw::pack(datastream, number_of_transactions);
  for (const std::shared_ptr<full_transaction_type>& full_transaction : full_transactions)
  {
    const serialized_transaction_data& serialized_transaction = full_transaction->get_serialized_transaction();
    datastream.write(serialized_transaction.begin, serialized_transaction.signed_transaction_end - serialized_transaction.begin);
  }
  FC_ASSERT(datastream.remaining() == 0, "Error: data leftover after encoding block");

  // serialization done.  Fill out the decoded version of the block with the transactions
  new_block.transactions.reserve(full_transactions.size());
  for (const std::shared_ptr<full_transaction_type>& full_transaction : full_transactions)
    new_block.transactions.push_back(full_transaction->get_transaction());

  // and keep a copy of the full_transactions
  full_block->full_transactions = full_transactions;

  full_block->has_uncompressed_block.store(true, std::memory_order_release);
  full_block->has_unpacked_block_header.store(true, std::memory_order_release);
  full_block->has_unpacked_block.store(true, std::memory_order_release);

  return full_block;
} FC_CAPTURE_AND_RETHROW() }

void full_block_type::decompress_block() const
{
  std::lock_guard<std::mutex> guard(uncompressed_block_mutex);
  if (!has_uncompressed_block.load(std::memory_order_consume))
  {
    assert(!decoded_block_storage);
    FC_ASSERT(!decoded_block_storage, "Block is already decompressed");

    assert(has_compressed_block);
    FC_ASSERT(has_compressed_block.load(std::memory_order_consume), "Nothing to decompress");

    decoded_block_storage = std::make_shared<decoded_block_storage_type>();
    std::tie(decoded_block_storage->uncompressed_block.raw_bytes, decoded_block_storage->uncompressed_block.raw_size) = 
      block_log::decompress_raw_block(compressed_block.compressed_bytes.get(), 
                                      compressed_block.compressed_size, 
                                      compressed_block.compression_attributes);

    has_uncompressed_block.store(true, std::memory_order_release);
  }
}

const uncompressed_block_data& full_block_type::get_uncompressed_block() const
{
  if (!has_uncompressed_block.load(std::memory_order_consume))
  {
    fc::time_point wait_begin = fc::time_point::now();
    decompress_block();
    fc::time_point wait_end = fc::time_point::now();
    fc::microseconds wait_duration = wait_end - wait_begin;
    fc_ilog(fc::logger::get("worker_thread"), "waited ${wait_duration}μs in full_block_type::get_uncompressed_block()", (wait_duration));
  }
  return decoded_block_storage->uncompressed_block;
}

bool full_block_type::has_compressed_block_data() const
{
  return has_compressed_block.load(std::memory_order_relaxed); 
}

void full_block_type::compress_block() const
{
  std::lock_guard<std::mutex> guard(compressed_block_mutex);
  if (!has_compressed_block.load(std::memory_order_consume))
  {
    assert(decoded_block_storage);
    FC_ASSERT(decoded_block_storage, "can't compress unless I already have the uncompressed version");

    fc::time_point start = fc::time_point::now();

    fc::optional<uint8_t> dictionary_number_to_use = hive::chain::get_best_available_zstd_compression_dictionary_number_for_block(get_block_num());
    std::tie(compressed_block.compressed_bytes, compressed_block.compressed_size) = 
      block_log::compress_block_zstd(decoded_block_storage->uncompressed_block.raw_bytes.get(), decoded_block_storage->uncompressed_block.raw_size, dictionary_number_to_use);
    compressed_block.compression_attributes.flags = hive::chain::block_log::block_flags::zstd;
    compressed_block.compression_attributes.dictionary_number = dictionary_number_to_use;

    compression_time = fc::time_point::now() - start;
    has_compressed_block.store(true, std::memory_order_release);
  }
}

const compressed_block_data& full_block_type::get_compressed_block() const
{
  if (!has_compressed_block.load(std::memory_order_consume))
  {
    fc::time_point wait_begin = fc::time_point::now();
    compress_block();
    fc::time_point wait_end = fc::time_point::now();
    fc::microseconds wait_duration = wait_end - wait_begin;
    fc_ilog(fc::logger::get("worker_thread"), "waited ${wait_duration}μs in full_block_type::get_compressed_block(), compression took ${compression_time}μs",
            (wait_duration)(compression_time));
  }
  return compressed_block;
}

/* static */ block_id_type full_block_type::construct_block_id(const char* signed_block_header_begin, size_t signed_block_header_size, uint32_t block_num)
{
  // to get the block id, we start by taking the hash of the header
  fc::sha224 block_hash = fc::sha224::hash(signed_block_header_begin, signed_block_header_size);
  // then overwrite the first four bytes of the hash with the block num
  block_hash._hash[0] = fc::endian_reverse_u32(block_num);

  // our block_id is the first 20 bytes of that result (discarding the last 8 bytes of the hash)
  block_id_type block_id;
  memcpy(block_id._hash, block_hash._hash, std::min(sizeof(block_id_type), sizeof(block_hash)));
  return block_id;
}

void full_block_type::decode_block_header() const
{
  std::lock_guard<std::mutex> guard(unpacked_block_header_mutex);
  if (!has_unpacked_block_header.load(std::memory_order_consume))
  {
    decompress_block(); // force decompression to happen now, if it hasn't happened yet

    fc::time_point decode_block_header_begin = fc::time_point::now();
    assert(!decoded_block_storage->block);
    FC_ASSERT(!decoded_block_storage->block, "It seems like the block header has already been unpacked");
    
    // unpack the block header.  We're likley to eventually want either or both of the block_id or 
    // the block_digest, and a little work now can make those cheaper to compute in the 
    // future.
    //
    // The block hierarchy looks like:
    //   block_header (most of the fields in the block)
    //   +- signed_block_header (adds `witness_signature`)
    //      +- signed_block (adds `transactions`)
    //
    // The block_digest, used in witness signature verification, is the sha256 of the serialized
    // block header.
    // The block_id is the sha224 of the signed_block_header, with the first four bytes replaced
    // with the block number.
    // To generate those, we'll need to know how many bytes of the uncompressed_block we should
    // feed in to the hash function.  So instead of deserializing the whole signed_block in
    // one go, we do the individual parts so we can store off the sizes along the way.

    decoded_block_storage->block = signed_block();
    fc::datastream<const char*> datastream(decoded_block_storage->uncompressed_block.raw_bytes.get(), 
                                           decoded_block_storage->uncompressed_block.raw_size);
    fc::raw::unpack(datastream, (block_header&)*decoded_block_storage->block);
    block_header_size = datastream.tellp();
    fc::raw::unpack(datastream, decoded_block_storage->block->witness_signature);
    signed_block_header_size = datastream.tellp();
    
    // construct the block id (not always needed, but we assume it's cheap enough that it's not worth doing lazily)
    if (!block_id)
      block_id = construct_block_id(decoded_block_storage->uncompressed_block.raw_bytes.get(), signed_block_header_size, decoded_block_storage->block->block_num());
    
    // construct the block digest (also not always needed, but we assume it's cheap enough that it's not worth doing lazily)
    if (!digest)
    {
      // digest is just the hash of the block_header
      digest = digest_type::hash(decoded_block_storage->uncompressed_block.raw_bytes.get(), block_header_size);
    }
    
    fc::time_point decode_block_header_end = fc::time_point::now();
    decode_block_header_time = decode_block_header_end - decode_block_header_begin;

    has_unpacked_block_header.store(true, std::memory_order_release);
  }
}

const signed_block_header& full_block_type::get_block_header() const
{
  if (!has_unpacked_block_header.load(std::memory_order_consume))
  {
    fc::time_point decode_begin = fc::time_point::now();
    decode_block_header();
    fc::time_point decode_end = fc::time_point::now();
    fc::microseconds decode_duration = decode_end - decode_begin;
    fc_ilog(fc::logger::get("worker_thread"), "waited ${decode_duration}μs in full_block_type::get_block_header(), header decode took ${decode_block_header_time}μs",
            (decode_duration)(decode_block_header_time));
  }

  return *decoded_block_storage->block;
}



void full_block_type::decode_block() const
{
  std::lock_guard<std::mutex> guard(unpacked_block_mutex);
  if (!has_unpacked_block.load(std::memory_order_consume))
  {
    decode_block_header(); // force decompression and decoding through the block header to happen now, if it hasn't happened yet

    fc::time_point decode_block_begin = fc::time_point::now();
    // decoded_block_storage->block should now have the signed_block_header slice valid, the remainder (the transactions) empty
    assert(decoded_block_storage->block);
    FC_ASSERT(decoded_block_storage->block, "The block header should have already been unpacked, but I don't see it");
    
    // now the only part left is the vector of transactions.  we could just fc::raw::unpack(datastream, block->transactions),
    // but we need to know where each transaction begins and ends because we need to preserve the serialized form
    // of each transaction.
    // A vector serialized as a size followed by the transactions themselves
    fc::datastream<char*> datastream(decoded_block_storage->uncompressed_block.raw_bytes.get(), 
                                     decoded_block_storage->uncompressed_block.raw_size);
    datastream.seekp(signed_block_header_size);
    fc::unsigned_int number_of_transactions;
    fc::raw::unpack(datastream, number_of_transactions);
    decoded_block_storage->block->transactions.resize(number_of_transactions.value);
    full_transactions.resize(number_of_transactions.value);
    for (unsigned i = 0; i < number_of_transactions.value; ++i)
    {
      // The transaction hierarchy is:
      // transaction
      // +- signed_transaction (adds a vector<signatures>)
      // 
      // the transaction digest, transaction_id, and sig_digest are simply computed the fields in the `transaction` base 
      // class.  The `merkle_digest()` also includes the signatures, so just like the block, we need to decode the 
      // transaction in parts and record off the start/end points
      serialized_transaction_data serialized_transaction;
      serialized_transaction.begin = decoded_block_storage->uncompressed_block.raw_bytes.get() + datastream.tellp();
      fc::raw::unpack(datastream, (transaction&)decoded_block_storage->block->transactions[i]);
      serialized_transaction.transaction_end = decoded_block_storage->uncompressed_block.raw_bytes.get() + datastream.tellp();
      fc::raw::unpack(datastream, decoded_block_storage->block->transactions[i].signatures);
      serialized_transaction.signed_transaction_end = decoded_block_storage->uncompressed_block.raw_bytes.get() + datastream.tellp();
    
      // if we're in live mode (as opposed to not syncing/replaying), use the transaction cache to see if transactions in this
      // block were previously seen as standalone transactions.  If so, we can reuse their data and avoid re-validating the transaction
      const bool use_transaction_cache = fc::time_point::now() - decoded_block_storage->block->timestamp < fc::minutes(1);
      full_transactions[i] = full_transaction_type::create_from_block(decoded_block_storage, i, serialized_transaction, use_transaction_cache);
    }
    FC_ASSERT(datastream.remaining() == 0, "Error: data leftover after decoding block");

    fc::time_point decode_block_end = fc::time_point::now();
    decode_block_time = decode_block_end - decode_block_begin;

    has_unpacked_block.store(true, std::memory_order_release);
  }
}

const signed_block& full_block_type::get_block() const
{
  if (!has_unpacked_block.load(std::memory_order_consume))
  {
    fc::time_point decode_begin = fc::time_point::now();
    decode_block();
    fc::time_point decode_end = fc::time_point::now();
    fc::microseconds decode_duration = decode_end - decode_begin;
    fc_ilog(fc::logger::get("worker_thread"), "waited ${decode_duration}μs in full_block_type::get_block(), actual decode took ${decode_block_time}μs", 
            (decode_duration)(decode_block_time));
  }

  return *decoded_block_storage->block;
}

const block_id_type& full_block_type::get_block_id() const 
{
  (void)get_block_header();
  return *block_id;
}

uint32_t full_block_type::get_block_num() const
{
  return block_header::num_from_id(get_block_id());
}

void full_block_type::compute_legacy_block_message_hash() const
{
  std::lock_guard<std::mutex> guard(legacy_block_message_hash_mutex);
  if (!has_legacy_block_message_hash.load(std::memory_order_consume))
  {
    decode_block_header();
    // compute the legacy_block_message_hash.  This is the hash you would get if you packed this block into a
    // block_message and asked for its message_id.  On the p2p net, we'll always use the legacy_message_hash, even
    // when we're sending blocks in their compressed form.
    fc::time_point compute_hash_begin = fc::time_point::now();
    fc::ripemd160::encoder legacy_message_hash_encoder;
    legacy_message_hash_encoder.write(decoded_block_storage->uncompressed_block.raw_bytes.get(), decoded_block_storage->uncompressed_block.raw_size);
    legacy_message_hash_encoder.write(block_id->data(), block_id->data_size());
    legacy_block_message_hash = legacy_message_hash_encoder.result();
    fc::time_point compute_hash_end = fc::time_point::now();
    compute_legacy_block_message_hash_time = compute_hash_end - compute_hash_begin;

    has_legacy_block_message_hash.store(true, std::memory_order_release);
  }
}

const fc::ripemd160& full_block_type::get_legacy_block_message_hash() const
{
  if (!has_legacy_block_message_hash.load(std::memory_order_consume))
  {
    fc::time_point wait_begin = fc::time_point::now();
    compute_legacy_block_message_hash();
    fc::time_point wait_end = fc::time_point::now();
    fc::microseconds wait_duration = wait_end - wait_begin;
    fc_ilog(fc::logger::get("worker_thread"), "waited ${wait_duration}μs in full_block_type::get_legacy_block_message_hash(), computing it took ${compute_legacy_block_message_hash_time}μs", 
            (wait_duration)(compute_legacy_block_message_hash_time));
  }
  return legacy_block_message_hash;
}

void full_block_type::compute_signing_key() const
{
  std::lock_guard<std::mutex> guard(block_signing_key_merkle_root_mutex);
  if (!block_signing_key)
  {
    decode_block_header();
    fc::time_point compute_begin = fc::time_point::now();
    const transaction_signature_validation_rules_type& validation_rules = get_transaction_signature_validation_rules_at_time(decoded_block_storage->block->timestamp);
    block_signing_key = hive::protocol::signed_block_header::signee(decoded_block_storage->block->witness_signature, *digest, validation_rules.signature_type);
    fc::time_point compute_end = fc::time_point::now();
    compute_block_signing_key_time = compute_end - compute_begin;
  }
}

const fc::ecc::public_key& full_block_type::get_signing_key() const
{
  {
    std::lock_guard<std::mutex> guard(block_signing_key_merkle_root_mutex);
    if (block_signing_key)
      return *block_signing_key;
  }

  fc::time_point wait_begin = fc::time_point::now();
  compute_signing_key();
  fc::time_point wait_end = fc::time_point::now();
  fc::microseconds wait_duration = wait_end - wait_begin;
  fc_ilog(fc::logger::get("worker_thread"), "waited ${wait_duration}μs in full_block_type::get_signing_key(), actual calculation took ${compute_block_signing_key_time}μs",
          (wait_duration)(compute_block_signing_key_time));
  return *block_signing_key;
}

const std::vector<std::shared_ptr<full_transaction_type>>& full_block_type::get_full_transactions() const
{
  get_block();
  return full_transactions;
}

uint32_t full_block_type::get_uncompressed_block_size() const
{
  get_uncompressed_block();
  return decoded_block_storage->uncompressed_block.raw_size;
}

/* static */ checksum_type full_block_type::compute_merkle_root(const std::vector<std::shared_ptr<full_transaction_type>>& full_transactions)
{
  if (full_transactions.empty())
    return checksum_type();

  std::vector<digest_type> ids;
  ids.resize(full_transactions.size());
  std::transform(full_transactions.begin(), full_transactions.end(), ids.begin(), 
                 [](const std::shared_ptr<full_transaction_type>& transaction) { return transaction->get_merkle_digest(); });

  hive::protocol::serialization_mode_controller::pack_guard guard(hive::protocol::pack_type::legacy);

  vector<digest_type>::size_type current_number_of_hashes = ids.size();
  while (current_number_of_hashes > 1)
  {
    // hash ID's in pairs
    uint32_t i_max = current_number_of_hashes - (current_number_of_hashes & 1);
    uint32_t k = 0;

    for (uint32_t i = 0; i < i_max; i += 2)
      ids[k++] = digest_type::hash(std::make_pair(ids[i], ids[i + 1]));

    if (current_number_of_hashes & 1)
      ids[k++] = ids[i_max];
    current_number_of_hashes = k;
  }

  return checksum_type::hash(ids[0]);
}

void full_block_type::compute_merkle_root() const
{
  std::lock_guard<std::mutex> guard(block_signing_key_merkle_root_mutex);
  if (!merkle_root)
  {
    decode_block();
    fc::time_point compute_begin = fc::time_point::now();
    merkle_root = compute_merkle_root(full_transactions);
    fc::time_point compute_end = fc::time_point::now();
    compute_merkle_root_time = compute_end - compute_begin;
  }
}

const checksum_type& full_block_type::get_merkle_root() const
{ try {
  {
    std::lock_guard<std::mutex> guard(block_signing_key_merkle_root_mutex);
    if (merkle_root)
      return *merkle_root;
  }

  fc::time_point wait_begin = fc::time_point::now();
  compute_merkle_root();
  fc::time_point wait_end = fc::time_point::now();
  fc::microseconds wait_duration = wait_end - wait_begin;
  fc_ilog(fc::logger::get("worker_thread"), "waited ${wait_duration}μs in full_block_type::get_merkle_root(), actual calculation took ${compute_merkle_root_time}μs",
          (wait_duration)(compute_merkle_root_time));
  return *merkle_root;
} FC_CAPTURE_AND_RETHROW() }

} } // end namespace hive::chain
