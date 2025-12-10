/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <graphene/net/core_messages.hpp>
#include <graphene/net/message.hpp>
#include <hive/chain/full_block.hpp>
#include <hive/chain/blockchain_worker_thread_pool.hpp>
#include <fc/io/raw.hpp>

namespace graphene { namespace net {

  //
  // Legacy serialization helpers for backward compatibility with protocol v107.
  // These pack messages using the old IP address format (no family byte).
  //

  namespace {
    // Pack hello_message in legacy format
    void pack_hello_message_legacy(std::vector<char>& buffer, const hello_message& msg)
    {
      fc::datastream<size_t> ps;
      fc::raw::pack(ps, msg.user_agent);
      fc::raw::pack(ps, msg.core_protocol_version);
      fc::raw::pack_legacy(ps, msg.inbound_address);
      fc::raw::pack(ps, msg.inbound_port);
      fc::raw::pack(ps, msg.outbound_port);
      fc::raw::pack(ps, msg.node_public_key);
      fc::raw::pack(ps, msg.signed_shared_secret);
      fc::raw::pack(ps, msg.user_data);
      buffer.resize(ps.tellp());

      fc::datastream<char*> ds(buffer.data(), buffer.size());
      fc::raw::pack(ds, msg.user_agent);
      fc::raw::pack(ds, msg.core_protocol_version);
      fc::raw::pack_legacy(ds, msg.inbound_address);
      fc::raw::pack(ds, msg.inbound_port);
      fc::raw::pack(ds, msg.outbound_port);
      fc::raw::pack(ds, msg.node_public_key);
      fc::raw::pack(ds, msg.signed_shared_secret);
      fc::raw::pack(ds, msg.user_data);
    }

    // Pack address_info in legacy format
    template<typename Stream>
    void pack_address_info_legacy(Stream& s, const address_info& info)
    {
      fc::raw::pack_legacy(s, info.remote_endpoint);
      fc::raw::pack(s, info.last_seen_time);
      fc::raw::pack(s, info.latency);
      fc::raw::pack(s, info.node_id);
      fc::raw::pack(s, info.direction);
      fc::raw::pack(s, info.firewalled);
    }

    // Pack address_message in legacy format (only includes IPv4 addresses)
    void pack_address_message_legacy(std::vector<char>& buffer, const address_message& msg)
    {
      // First, filter to only IPv4 addresses
      std::vector<address_info> ipv4_addresses;
      for (const auto& addr : msg.addresses) {
        if (addr.remote_endpoint.get_address().is_ipv4()) {
          ipv4_addresses.push_back(addr);
        }
      }

      // Calculate size
      fc::datastream<size_t> ps;
      fc::raw::pack(ps, fc::unsigned_int(ipv4_addresses.size()));
      for (const auto& addr : ipv4_addresses) {
        pack_address_info_legacy(ps, addr);
      }
      buffer.resize(ps.tellp());

      // Pack data
      fc::datastream<char*> ds(buffer.data(), buffer.size());
      fc::raw::pack(ds, fc::unsigned_int(ipv4_addresses.size()));
      for (const auto& addr : ipv4_addresses) {
        pack_address_info_legacy(ds, addr);
      }
    }

    // Pack connection_rejected_message in legacy format
    void pack_connection_rejected_message_legacy(std::vector<char>& buffer, const connection_rejected_message& msg)
    {
      fc::datastream<size_t> ps;
      fc::raw::pack(ps, msg.user_agent);
      fc::raw::pack(ps, msg.core_protocol_version);
      fc::raw::pack_legacy(ps, msg.remote_endpoint);
      fc::raw::pack(ps, msg.reason_code);
      fc::raw::pack(ps, msg.reason_string);
      buffer.resize(ps.tellp());

      fc::datastream<char*> ds(buffer.data(), buffer.size());
      fc::raw::pack(ds, msg.user_agent);
      fc::raw::pack(ds, msg.core_protocol_version);
      fc::raw::pack_legacy(ds, msg.remote_endpoint);
      fc::raw::pack(ds, msg.reason_code);
      fc::raw::pack(ds, msg.reason_string);
    }

    // Pack check_firewall_message in legacy format
    void pack_check_firewall_message_legacy(std::vector<char>& buffer, const check_firewall_message& msg)
    {
      fc::datastream<size_t> ps;
      fc::raw::pack(ps, msg.node_id);
      fc::raw::pack_legacy(ps, msg.endpoint_to_check);
      buffer.resize(ps.tellp());

      fc::datastream<char*> ds(buffer.data(), buffer.size());
      fc::raw::pack(ds, msg.node_id);
      fc::raw::pack_legacy(ds, msg.endpoint_to_check);
    }

    // Pack check_firewall_reply_message in legacy format
    void pack_check_firewall_reply_message_legacy(std::vector<char>& buffer, const check_firewall_reply_message& msg)
    {
      fc::datastream<size_t> ps;
      fc::raw::pack(ps, msg.node_id);
      fc::raw::pack_legacy(ps, msg.endpoint_checked);
      fc::raw::pack(ps, msg.result);
      buffer.resize(ps.tellp());

      fc::datastream<char*> ds(buffer.data(), buffer.size());
      fc::raw::pack(ds, msg.node_id);
      fc::raw::pack_legacy(ds, msg.endpoint_checked);
      fc::raw::pack(ds, msg.result);
    }

  } // anonymous namespace

  //
  // Legacy deserialization helpers - for parsing messages from v107 peers
  //

  hello_message unpack_hello_message(const std::vector<char>& data)
  {
    hello_message result;
    fc::datastream<const char*> ds(data.data(), data.size());

    // Parse fields up to and including core_protocol_version
    fc::raw::unpack(ds, result.user_agent, 0);
    fc::raw::unpack(ds, result.core_protocol_version, 0);

    // Now we know the version - use appropriate format for the rest
    if (result.core_protocol_version >= GRAPHENE_NET_PROTOCOL_IPV6_VERSION) {
      // New format with family byte
      fc::raw::unpack(ds, result.inbound_address, 0);
    } else {
      // Legacy format - just uint32_t
      fc::raw::unpack_legacy(ds, result.inbound_address, 0);
    }

    fc::raw::unpack(ds, result.inbound_port, 0);
    fc::raw::unpack(ds, result.outbound_port, 0);
    fc::raw::unpack(ds, result.node_public_key, 0);
    fc::raw::unpack(ds, result.signed_shared_secret, 0);
    fc::raw::unpack(ds, result.user_data, 0);

    return result;
  }

  address_info unpack_address_info_legacy(fc::datastream<const char*>& ds)
  {
    address_info result;
    fc::raw::unpack_legacy(ds, result.remote_endpoint, 0);
    fc::raw::unpack(ds, result.last_seen_time, 0);
    fc::raw::unpack(ds, result.latency, 0);
    fc::raw::unpack(ds, result.node_id, 0);
    fc::raw::unpack(ds, result.direction, 0);
    fc::raw::unpack(ds, result.firewalled, 0);
    return result;
  }

  address_message unpack_address_message(const std::vector<char>& data, bool peer_supports_ipv6)
  {
    address_message result;
    fc::datastream<const char*> ds(data.data(), data.size());

    if (peer_supports_ipv6) {
      // New format
      fc::raw::unpack(ds, result, 0);
    } else {
      // Legacy format
      fc::unsigned_int count;
      fc::raw::unpack(ds, count, 0);
      result.addresses.reserve(count.value);
      for (uint32_t i = 0; i < count.value; ++i) {
        result.addresses.push_back(unpack_address_info_legacy(ds));
      }
    }

    return result;
  }

  connection_rejected_message unpack_connection_rejected_message(const std::vector<char>& data, bool peer_supports_ipv6)
  {
    connection_rejected_message result;
    fc::datastream<const char*> ds(data.data(), data.size());

    fc::raw::unpack(ds, result.user_agent, 0);
    fc::raw::unpack(ds, result.core_protocol_version, 0);

    if (peer_supports_ipv6) {
      fc::raw::unpack(ds, result.remote_endpoint, 0);
    } else {
      fc::raw::unpack_legacy(ds, result.remote_endpoint, 0);
    }

    fc::raw::unpack(ds, result.reason_code, 0);
    fc::raw::unpack(ds, result.reason_string, 0);

    return result;
  }

  check_firewall_message unpack_check_firewall_message(const std::vector<char>& data, bool peer_supports_ipv6)
  {
    check_firewall_message result;
    fc::datastream<const char*> ds(data.data(), data.size());

    fc::raw::unpack(ds, result.node_id, 0);

    if (peer_supports_ipv6) {
      fc::raw::unpack(ds, result.endpoint_to_check, 0);
    } else {
      fc::raw::unpack_legacy(ds, result.endpoint_to_check, 0);
    }

    return result;
  }

  check_firewall_reply_message unpack_check_firewall_reply_message(const std::vector<char>& data, bool peer_supports_ipv6)
  {
    check_firewall_reply_message result;
    fc::datastream<const char*> ds(data.data(), data.size());

    fc::raw::unpack(ds, result.node_id, 0);

    if (peer_supports_ipv6) {
      fc::raw::unpack(ds, result.endpoint_checked, 0);
    } else {
      fc::raw::unpack_legacy(ds, result.endpoint_checked, 0);
    }

    fc::raw::unpack(ds, result.result, 0);

    return result;
  }

  message create_hello_message_for_peer(const hello_message& msg, bool peer_supports_ipv6)
  {
    message result;
    result.msg_type = hello_message::type;
    if (msg.inbound_address.is_ipv6()) {
      // If our inbound_address is IPv6, we are communicating over an IPv6 socket.
      // A v107 peer cannot accept IPv6 connections (they only listen on IPv4),
      // so if we're connected via IPv6, the peer MUST be v108+ and support the new format.
      // This resolves the apparent chicken-and-egg problem: we don't need to know the
      // peer's protocol version from their hello because the transport layer tells us.
      result.data = fc::raw::pack_to_vector(msg);
    } else if (peer_supports_ipv6) {
      // IPv4 address to known v108+ peer - use new format for consistency
      result.data = fc::raw::pack_to_vector(msg);
    } else {
      // IPv4 address to legacy v107 peer (or unknown peer on first hello) - use legacy format
      pack_hello_message_legacy(result.data, msg);
    }
    result.size = result.data.size();
    return result;
  }

  message create_address_message_for_peer(const address_message& msg, bool peer_supports_ipv6)
  {
    message result;
    result.msg_type = address_message::type;
    if (peer_supports_ipv6) {
      // New format - use standard serialization
      result.data = fc::raw::pack_to_vector(msg);
    } else {
      // Legacy format - filter out IPv6 and use old serialization
      pack_address_message_legacy(result.data, msg);
    }
    result.size = result.data.size();
    return result;
  }

  message create_connection_rejected_message_for_peer(const connection_rejected_message& msg, bool peer_supports_ipv6)
  {
    message result;
    result.msg_type = connection_rejected_message::type;
    if (peer_supports_ipv6 || !msg.remote_endpoint.get_address().is_ipv4()) {
      // For new peers or if we somehow have an IPv6 endpoint (shouldn't happen for old peers)
      result.data = fc::raw::pack_to_vector(msg);
    } else {
      pack_connection_rejected_message_legacy(result.data, msg);
    }
    result.size = result.data.size();
    return result;
  }

  message create_check_firewall_message_for_peer(const check_firewall_message& msg, bool peer_supports_ipv6)
  {
    message result;
    result.msg_type = check_firewall_message::type;
    if (peer_supports_ipv6 || !msg.endpoint_to_check.get_address().is_ipv4()) {
      result.data = fc::raw::pack_to_vector(msg);
    } else {
      pack_check_firewall_message_legacy(result.data, msg);
    }
    result.size = result.data.size();
    return result;
  }

  message create_check_firewall_reply_message_for_peer(const check_firewall_reply_message& msg, bool peer_supports_ipv6)
  {
    message result;
    result.msg_type = check_firewall_reply_message::type;
    if (peer_supports_ipv6 || !msg.endpoint_checked.get_address().is_ipv4()) {
      result.data = fc::raw::pack_to_vector(msg);
    } else {
      pack_check_firewall_reply_message_legacy(result.data, msg);
    }
    result.size = result.data.size();
    return result;
  }

  const core_message_type_enum trx_message::type                             = core_message_type_enum::trx_message_type;
  const core_message_type_enum block_message::type                           = core_message_type_enum::block_message_type;
  const core_message_type_enum compressed_block_message::type                = core_message_type_enum::compressed_block_message_type;
  const core_message_type_enum item_ids_inventory_message::type              = core_message_type_enum::item_ids_inventory_message_type;
  const core_message_type_enum blockchain_item_ids_inventory_message::type   = core_message_type_enum::blockchain_item_ids_inventory_message_type;
  const core_message_type_enum fetch_blockchain_item_ids_message::type       = core_message_type_enum::fetch_blockchain_item_ids_message_type;
  const core_message_type_enum fetch_items_message::type                     = core_message_type_enum::fetch_items_message_type;
  const core_message_type_enum item_not_available_message::type              = core_message_type_enum::item_not_available_message_type;
  const core_message_type_enum hello_message::type                           = core_message_type_enum::hello_message_type;
  const core_message_type_enum connection_accepted_message::type             = core_message_type_enum::connection_accepted_message_type;
  const core_message_type_enum connection_rejected_message::type             = core_message_type_enum::connection_rejected_message_type;
  const core_message_type_enum address_request_message::type                 = core_message_type_enum::address_request_message_type;
  const core_message_type_enum address_message::type                         = core_message_type_enum::address_message_type;
  const core_message_type_enum closing_connection_message::type              = core_message_type_enum::closing_connection_message_type;
  const core_message_type_enum current_time_request_message::type            = core_message_type_enum::current_time_request_message_type;
  const core_message_type_enum current_time_reply_message::type              = core_message_type_enum::current_time_reply_message_type;
  const core_message_type_enum check_firewall_message::type                  = core_message_type_enum::check_firewall_message_type;
  const core_message_type_enum check_firewall_reply_message::type            = core_message_type_enum::check_firewall_reply_message_type;
  const core_message_type_enum get_current_connections_request_message::type = core_message_type_enum::get_current_connections_request_message_type;
  const core_message_type_enum get_current_connections_reply_message::type   = core_message_type_enum::get_current_connections_reply_message_type;

  message::message(const block_message& msg)
  {
    assert(msg.full_block);
    FC_ASSERT(msg.full_block, "Can't send a block_message when we don't have the full_block");
    msg_type = block_message::type;
    const hive::chain::uncompressed_block_data& uncompressed_data = msg.full_block->get_uncompressed_block();
    size = uncompressed_data.raw_size + sizeof(block_id_type);
    data.resize(size);
    memcpy(data.data(), uncompressed_data.raw_bytes.get(), uncompressed_data.raw_size);
    memcpy(data.data() + uncompressed_data.raw_size, &msg.full_block->get_block_id(), sizeof(block_id_type));
  }

  block_message message::as_block_message( hive::chain::blockchain_worker_thread_pool& thread_pool ) const
  {
    try {
      FC_ASSERT(msg_type == block_message::type);

      // an old-style block_message was serialized version of the signed_block, followed by the block_id
      // we've changed the block_message to contain a full_block record, but we still need to
      // deserialize the old format from the network.
      // When deserializing it as a full block, we just want to take the bytes from the message and
      // stuff them into the full_block's uncompressed_data field.
      // The block_id part is just a ripemd160 at the end, so we'll lop that off.

      FC_ASSERT(data.size() > sizeof(block_id_type));
      hive::chain::uncompressed_block_data uncompressed_block;
      uncompressed_block = hive::chain::uncompressed_block_data();
      uncompressed_block.raw_size = data.size() - sizeof(block_id_type);
      uncompressed_block.raw_bytes.reset(new char[uncompressed_block.raw_size]);
      memcpy(uncompressed_block.raw_bytes.get(), data.data(), uncompressed_block.raw_size);

      // the remainder of the message is the block_id, but the block_message actually gets this
      // out of the block data itself, so we can just ignore it instead of unpacking it
      // memcpy(&message.block_id, data.data() + uncompressed_block.raw_size, sizeof(block_id_type));

      // begin processing the block in the worker threads
      std::shared_ptr<full_block_type> full_block = full_block_type::create_from_uncompressed_block_data(std::move(uncompressed_block.raw_bytes), uncompressed_block.raw_size);
      thread_pool.enqueue_work(full_block, hive::chain::blockchain_worker_thread_pool::data_source_type::block_received_from_p2p);

      return block_message(full_block);
    } FC_RETHROW_EXCEPTIONS(warn, "error unpacking network message as a block_message");
  }

  namespace
  {
    constexpr size_t COMPRESSED_BLOCK_COMPRESSION_METADATA_SIZE = 2;
  }

  message::message(const hive::chain::compressed_block_data& compressed_data)
  {
    msg_type = compressed_block_message::type;

    // we need two extra bytes to specify whether the block is compressed, and if it is, what dictionary
    // to use to decompress it (same data as is stored in the block_log's index)
    data.resize(COMPRESSED_BLOCK_COMPRESSION_METADATA_SIZE + compressed_data.compressed_size);
    if (compressed_data.compression_attributes.flags == hive::chain::block_flags::zstd)
      data[0] |= 0x80;
    if (compressed_data.compression_attributes.dictionary_number)
    {
      data[0] |= 0x40;
      data[1] = *compressed_data.compression_attributes.dictionary_number;
    }

    memcpy(&data[COMPRESSED_BLOCK_COMPRESSION_METADATA_SIZE], compressed_data.compressed_bytes.get(), compressed_data.compressed_size);
    size = data.size();
  }

  compressed_block_message message::as_compressed_block_message( hive::chain::blockchain_worker_thread_pool& thread_pool ) const
  {
    try {
      FC_ASSERT(msg_type == compressed_block_message::type);

      // a compressed_block_message is serialized as two bytes of compression flags,
      // followed by the raw block of compressed data

      FC_ASSERT(data.size() > COMPRESSED_BLOCK_COMPRESSION_METADATA_SIZE);

      // decode the block attributes in the first two bytes
      hive::chain::block_attributes_t block_attributes;
      block_attributes.flags = (data[0] & 0x80) ? hive::chain::block_flags::zstd : hive::chain::block_flags::uncompressed;
      if (data[0] & 0x40)
        block_attributes.dictionary_number = data[1];

      // now decode the size of the data
      const size_t compressed_data_size = data.size() - COMPRESSED_BLOCK_COMPRESSION_METADATA_SIZE;
      std::unique_ptr<char[]> compressed_data_bytes(new char[compressed_data_size]);
      memcpy(compressed_data_bytes.get(), &data[COMPRESSED_BLOCK_COMPRESSION_METADATA_SIZE], compressed_data_size);

      // begin processing the block in the worker threads
      std::shared_ptr<full_block_type> full_block = full_block_type::create_from_compressed_block_data(std::move(compressed_data_bytes), 
                                                                                                       compressed_data_size, 
                                                                                                       block_attributes);
      thread_pool.enqueue_work(full_block, hive::chain::blockchain_worker_thread_pool::data_source_type::block_received_from_p2p);

      return compressed_block_message(full_block);
    } FC_RETHROW_EXCEPTIONS(warn, "error unpacking network message as a compressed_block_message");
  }

  message::message(const trx_message& msg)
  {
    assert(msg.full_transaction);
    FC_ASSERT(msg.full_transaction, "Can't send a transaction_message when we don't have the full_transaction");
    msg_type = trx_message::type;
    const hive::chain::serialized_transaction_data& serialized_tranaction = msg.full_transaction->get_serialized_transaction();
    size = serialized_tranaction.signed_transaction_end - serialized_tranaction.begin;
    data.resize(size);
    memcpy(data.data(), serialized_tranaction.begin, size);
  }

  trx_message message::as_trx_message( hive::chain::blockchain_worker_thread_pool& thread_pool ) const
  {
    try {
      FC_ASSERT(msg_type == trx_message::type);

      std::shared_ptr<hive::chain::full_transaction_type> full_transaction = hive::chain::full_transaction_type::create_from_serialized_transaction(data.data(), data.size(), 
                                                                                                                                                    true /* cache this transaction */);
      thread_pool.enqueue_work(full_transaction, hive::chain::blockchain_worker_thread_pool::data_source_type::standalone_transaction_received_from_p2p);

      return trx_message(full_transaction);
    } FC_RETHROW_EXCEPTIONS(warn, "error unpacking network message as a trx_message");
  }

} } // graphene::net

