#include <hive/chain/full_transaction.hpp>
#include <hive/chain/full_block.hpp>
#include <hive/protocol/exceptions.hpp>
#include <hive/protocol/hardfork.hpp>

namespace hive { namespace chain {

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
    signature_info = std::move(new_signature_info);
  }
  if (signature_info->signature_keys_exception)
    signature_info->signature_keys_exception->dynamic_rethrow_exception();
  return signature_info->signature_keys;
}

void full_transaction_type::validate(std::function<void(const hive::protocol::operation& op, bool post)> notify /* = std::function<void(const operation&, bool)>() */) const
{
  if (!validation_attempted)
  {
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
    validation_attempted = true;
  }
  if (validation_exception)
    validation_exception->dynamic_rethrow_exception();
}

const hive::protocol::required_authorities_type& full_transaction_type::get_required_authorities() const
{
  if (!required_authorities)
    required_authorities = hive::protocol::get_required_authorities(get_transaction().operations);
  return *required_authorities;
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
                                                                                             const serialized_transaction_data& serialized_transaction)
{
  std::shared_ptr<full_transaction_type> full_transaction = std::make_shared<full_transaction_type>();
  full_transaction->storage = contained_in_block_info{block_storage, index_in_block};
  full_transaction->serialized_transaction = serialized_transaction;
  return full_transaction;
}

/* static */ std::shared_ptr<full_transaction_type> full_transaction_type::create_from_signed_transaction(const hive::protocol::signed_transaction& transaction,
                                                                                                          hive::protocol::pack_type serialization_type)
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
  return full_transaction;
}

/* static */ std::shared_ptr<full_transaction_type> full_transaction_type::create_from_serialized_transaction(const char* raw_data, size_t size)
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

  return full_transaction;
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




} } // end namespace hive::chain
