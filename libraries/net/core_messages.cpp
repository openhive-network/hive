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


namespace graphene { namespace net {

  const core_message_type_enum trx_message::type                             = core_message_type_enum::trx_message_type;
  const core_message_type_enum block_message::type                           = core_message_type_enum::block_message_type;
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

  block_message message::as_block_message() const
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

      return block_message(full_block_type::create_from_uncompressed_block_data(std::move(uncompressed_block.raw_bytes), uncompressed_block.raw_size));
    } FC_RETHROW_EXCEPTIONS(warn, "error unpacking network message as a block_message");
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

  trx_message message::as_trx_message() const
  {
    try {
      FC_ASSERT(msg_type == trx_message::type);
      return trx_message(hive::chain::full_transaction_type::create_from_serialized_transaction(data.data(), data.size()));
    } FC_RETHROW_EXCEPTIONS(warn, "error unpacking network message as a trx_message");
  }

} } // graphene::net

