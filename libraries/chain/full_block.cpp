#include <hive/chain/full_block.hpp>
#include <hive/chain/block_compression_dictionaries.hpp>
#include <fc/bitutil.hpp>

namespace hive { namespace chain {

/* static */ std::atomic<uint32_t> full_block_type::number_of_instances_created = {0};
/* static */ std::atomic<uint32_t> full_block_type::number_of_instances_destroyed = {0};

full_block_type::full_block_type()
{
  ++number_of_instances_created;
  if (number_of_instances_created.load() % 10000 == 0)
    ilog("Currently ${count} full_blocks in memory", ("count", number_of_instances_created.load() - number_of_instances_destroyed.load()));
}

full_block_type::full_block_type(full_block_type&& rhs) :
  compressed_block(std::move(rhs.compressed_block)),
  decoded_block_storage(std::move(rhs.decoded_block_storage))
{
  ++number_of_instances_created;
  if (number_of_instances_created.load() % 10000 == 0)
    ilog("Currently ${count} full_blocks in memory", ("count", number_of_instances_created.load() - number_of_instances_destroyed.load()));
}

full_block_type::~full_block_type()
{
  ++number_of_instances_destroyed;
}

void full_block_type::decode()
{
  assert(!decoded_block_storage || !decoded_block_storage->block);
  FC_ASSERT(!decoded_block_storage || !decoded_block_storage->block, "Block is already decoded");
  assert(compressed_block || decoded_block_storage);
  FC_ASSERT(compressed_block || decoded_block_storage, "Nothing to decode");

  // if we haven't decompressed it yet, do it now
  if (!decoded_block_storage)
  {
    decoded_block_storage = std::make_shared<decoded_block_storage_type>();
    std::tie(decoded_block_storage->uncompressed_block.raw_bytes, decoded_block_storage->uncompressed_block.raw_size) = 
      block_log::decompress_raw_block(compressed_block->compressed_bytes.get(), 
                                      compressed_block->compressed_size, 
                                      compressed_block->compression_attributes);
  }

  // unpack the block.  We're likley to eventually want either or both of the block_id or 
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
  // now the only part left is the vector of transactions.  we could just fc::raw::unpack(datastream, block->transactions),
  // but we need to know where each transaction begins and ends because we need to preserve the serialized form
  // of each transaction.
  // A vector serialized as a size followed by the transactions themselves
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

    full_transactions[i] = full_transaction_type::create_from_block(decoded_block_storage, i, serialized_transaction);
  }
  FC_ASSERT(datastream.remaining() == 0, "Error: data leftover after decoding block");

  // right now, for simplicity, we'll force calculation of the block id and digest.
  // later, this should be done lazily
  get_block_id();
  get_digest();
}

/* static */ std::shared_ptr<full_block_type> full_block_type::create_from_compressed_block_data(std::unique_ptr<char[]>&& compressed_bytes, 
                                                                                                 size_t compressed_size,
                                                                                                 const block_log::block_attributes_t& compression_attributes)
{
  std::shared_ptr<full_block_type> full_block = std::make_shared<full_block_type>();
  full_block->compressed_block = compressed_block_data();
  full_block->compressed_block->compression_attributes = compression_attributes;
  full_block->compressed_block->compressed_bytes = std::move(compressed_bytes);
  full_block->compressed_block->compressed_size = compressed_size;

  full_block->decode();

  return full_block;
}

/* static */ std::shared_ptr<full_block_type> full_block_type::create_from_uncompressed_block_data(std::unique_ptr<char[]>&& raw_bytes, size_t raw_size)
{
  std::shared_ptr<full_block_type> full_block = std::make_shared<full_block_type>();

  full_block->decoded_block_storage = std::make_shared<decoded_block_storage_type>();
  full_block->decoded_block_storage->uncompressed_block = uncompressed_block_data{std::move(raw_bytes), raw_size};
  full_block->decode();
  return full_block;
}

// temporary helper to construct a full_block from a signed_block, to be deleted 
/* static */ std::shared_ptr<full_block_type> full_block_type::create_from_signed_block(const signed_block& block)
{
  // allocate enough storage to hold the block in its packed form
  std::shared_ptr<decoded_block_storage_type> decoded_block_storage = std::make_shared<decoded_block_storage_type>();
  decoded_block_storage->uncompressed_block.raw_size = fc::raw::pack_size(block);
  decoded_block_storage->uncompressed_block.raw_bytes.reset(new char[decoded_block_storage->uncompressed_block.raw_size]);

  // pack the block
  fc::datastream<char*> stream(decoded_block_storage->uncompressed_block.raw_bytes.get(), decoded_block_storage->uncompressed_block.raw_size);
  std::shared_ptr<full_block_type> full_block = std::make_shared<full_block_type>();
  full_block->decoded_block_storage = std::move(decoded_block_storage);
  fc::raw::pack(stream, block);

  // now decode it (we'll need some of the values we calculate along the way, that's why we're not just storing the existing unpacked block)
  full_block->decode();

  return full_block;
}

/* static */ std::shared_ptr<full_block_type> full_block_type::create_from_block_header_and_transactions(const block_header& header, 
                                                                                                         const std::vector<std::shared_ptr<full_transaction_type>>& full_transactions,
                                                                                                         const fc::ecc::private_key* signer)
{
  std::shared_ptr<full_block_type> full_block = std::make_shared<full_block_type>();

  // start constructing the full block.  The header should be completely filled out, except
  // for the merkle root.  Copy the header over and generate the merkle_root
  full_block->decoded_block_storage = std::make_shared<decoded_block_storage_type>();
  const std::shared_ptr<decoded_block_storage_type>& decoded_block_storage = full_block->decoded_block_storage; // alias

  decoded_block_storage->block = signed_block();
  signed_block& new_block = *decoded_block_storage->block; // alias to keep things shorter
  (block_header)new_block = header;
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

  // now that we've serialized the data used in computing the digest, use that to sign the block
  if (signer)
  {
    const transaction_signature_validation_rules_type& validation_rules = get_transaction_signature_validation_rules_at_time(header.timestamp);
    new_block.witness_signature = signer->sign_compact(full_block->get_digest(), validation_rules.signature_type);
  }
  // and serialize the signature
  fc::raw::pack(datastream, new_block.witness_signature);
  full_block->signed_block_header_size = datastream.tellp();

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

  // right now, for simplicity, we'll force calculation of the block id and digest.
  // later, this should be done lazily
  full_block->get_block_id();
  return full_block;
}

const uncompressed_block_data& full_block_type::get_uncompressed_block() const
{
  // eventually, this will decompress on demand
  assert(decoded_block_storage);
  FC_ASSERT(decoded_block_storage, "You can only get the uncompressed block after the block has been uncompressed");
  return decoded_block_storage->uncompressed_block;
}

const compressed_block_data& full_block_type::get_compressed_block() const
{
  if (!compressed_block)
  {
    assert(decoded_block_storage);
    FC_ASSERT(decoded_block_storage, "can't compress unless I already have the uncompressed version");
    fc::optional<uint8_t> dictionary_number_to_use = hive::chain::get_best_available_zstd_compression_dictionary_number_for_block(get_block_num());
    compressed_block = compressed_block_data();
    std::tie(compressed_block->compressed_bytes, compressed_block->compressed_size) = 
      block_log::compress_block_zstd(decoded_block_storage->uncompressed_block.raw_bytes.get(), decoded_block_storage->uncompressed_block.raw_size, dictionary_number_to_use);
    compressed_block->compression_attributes.flags = hive::chain::block_log::block_flags::zstd;
    compressed_block->compression_attributes.dictionary_number = dictionary_number_to_use;
  }
  return *compressed_block;
}

const signed_block& full_block_type::get_block() const
{
  // eventually, this will decode on demand
  assert(decoded_block_storage && decoded_block_storage->block);
  FC_ASSERT(decoded_block_storage && decoded_block_storage->block, "You can only get the block after the block has been decoded");
  return *decoded_block_storage->block;
}

const signed_block_header& full_block_type::get_block_header() const
{
  // eventually, this will decode on demand
  // it's possible that we could decode the block progressively, and only decode the block_header
  // portion when this is called, which is why this function exists (vs the user just calling the
  // full get_block() and slicing it).  That optimization may not make sense, so this is currently
  // identical to get_block()
  assert(decoded_block_storage && decoded_block_storage->block);
  FC_ASSERT(decoded_block_storage && decoded_block_storage->block, "You can only get the block_header after the block has been decoded");
  return *decoded_block_storage->block;
}

const block_id_type& full_block_type::get_block_id() const 
{
  // eventually, this will decode on demand
  assert(decoded_block_storage && decoded_block_storage->block);
  FC_ASSERT(decoded_block_storage && decoded_block_storage->block, "You can only get the block_id after the block has been decoded");
  if (!block_id)
  {
    // to get the block id, we start by taking the hash of the header
    fc::sha224 block_hash = fc::sha224::hash(decoded_block_storage->uncompressed_block.raw_bytes.get(), signed_block_header_size);
    // then overwrite the first four bytes of the hash with the block num
    block_hash._hash[0] = fc::endian_reverse_u32(decoded_block_storage->block->block_num());
    block_id = block_id_type();
    // our block_id is the first 20 bytes of that result (discarding the last 8 bytes of the hash)
    memcpy(block_id->_hash, block_hash._hash, std::min(sizeof(block_id_type), sizeof(block_hash)));
  }
  return *block_id;
}

uint32_t full_block_type::get_block_num() const
{
  return block_header::num_from_id(get_block_id());
}

const digest_type& full_block_type::get_digest() const 
{
  // eventually, this will decode on demand
  assert(decoded_block_storage && decoded_block_storage->block);
  FC_ASSERT(decoded_block_storage && decoded_block_storage->block, "You can only get the digest after the block has been decoded");
  // digest is just the hash of the block_header
  if (!digest)
    digest = digest_type::hash(decoded_block_storage->uncompressed_block.raw_bytes.get(), block_header_size);
  return *digest;
}

fc::ecc::public_key full_block_type::get_signing_key() const
{
  const signed_block_header& header = get_block_header();
  const transaction_signature_validation_rules_type& validation_rules = get_transaction_signature_validation_rules_at_time(header.timestamp);
  return hive::protocol::signed_block_header::signee(header.witness_signature, get_digest(), validation_rules.signature_type);
}

const std::vector<std::shared_ptr<full_transaction_type>>& full_block_type::get_full_transactions() const
{
  // eventually, this will decode the block on demand
  assert(decoded_block_storage);
  FC_ASSERT(decoded_block_storage, "You can only get the uncompressed block after the block has been uncompressed");
  return full_transactions;
}

uint32_t full_block_type::get_uncompressed_block_size() const
{
  // eventually, this will decompress on demand
  assert(decoded_block_storage);
  FC_ASSERT(decoded_block_storage, "You can only get the uncompressed size after the block has been decompressed");
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

const checksum_type& full_block_type::get_merkle_root() const
{
  if (!merkle_root)
    merkle_root = compute_merkle_root(full_transactions);
  return *merkle_root;
}

} } // end namespace hive::chain
