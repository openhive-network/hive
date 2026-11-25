#pragma once

#include <graphene/net/config.hpp>
#include <graphene/net/core_messages.hpp>
#include <graphene/net/node.hpp>

#include <hive/chain/full_block.hpp>
#include <hive/chain/full_transaction.hpp>

#include <fc/exception/exception.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/tag.hpp>

#include <functional>
#include <memory>

namespace graphene { namespace net {

  // Cache of recently-seen messages, kept so we can serve peers' fetch requests without
  // hitting deeper storage.  NOTE: with the introduction of authoritative chain-state checks
  // (classify_advertised_block_id / has_item at delivery), this cache is no longer the
  // arbiter of "have we seen this block?" for block-id-capable peers -- it retains that duty
  // only for legacy message-hash advertisements, whose hashes cannot be checked against the
  // chain.  Its block-clock expiry semantics are a serving-window bound, not a correctness
  // boundary.
  //
  // Lookups by message hash run on the p2p thread once per peer per relayed item, so that index
  // is hashed.  The contents-hash index stays ordered: it is off that path, and among items sharing
  // a contents hash (one transaction signed twice) its find() returns the first one cached.
  template <typename smart_ptr_type>
  class blockchain_tied_message_cache
  {
  private:
    static const uint32_t cache_duration_in_blocks = GRAPHENE_NET_MESSAGE_CACHE_DURATION_IN_BLOCKS;

    struct message_hash_index{};
    struct message_contents_hash_index{};
    struct block_clock_index{};

    struct message_info
    {
      smart_ptr_type item;
      uint32_t block_clock_when_received;

      // for network performance stats
      message_propagation_data propagation_data;

      message_info(const smart_ptr_type& item,
                   uint32_t block_clock_when_received,
                   const message_propagation_data& propagation_data) :
        item(item),
        block_clock_when_received(block_clock_when_received),
        propagation_data(propagation_data)
      {}
    };
    struct get_message_contents_hash
    {
      typedef item_hash_t result_type;
      const item_hash_t& get_hash_from_item(const std::shared_ptr<full_block_type>& full_block) const
      {
        return full_block->get_block_id();
      }
      const item_hash_t& get_hash_from_item(const std::shared_ptr<full_transaction_type>& full_transaction) const
      {
        return full_transaction->get_transaction_id();
      }
      const item_hash_t& operator()(const message_info& info) const
      {
        return get_hash_from_item(info.item);
      }
    };
    struct get_message_hash
    {
      typedef item_hash_t result_type;
      const item_hash_t& get_message_hash_from_item(const std::shared_ptr<full_block_type>& full_block) const
      {
        return full_block->get_legacy_block_message_hash();
      }
      const item_hash_t& get_message_hash_from_item(const std::shared_ptr<full_transaction_type>& full_transaction) const
      {
        return full_transaction->get_legacy_transaction_message_hash();
      }
      const item_hash_t& operator()(const message_info& info) const
      {
        return get_message_hash_from_item(info.item);
      }
    };
    typedef boost::multi_index_container
      <message_info,
       boost::multi_index::indexed_by<
         boost::multi_index::hashed_unique<boost::multi_index::tag<message_hash_index>, get_message_hash, p2p_item_hash>,
         boost::multi_index::ordered_non_unique<boost::multi_index::tag<message_contents_hash_index>, get_message_contents_hash>,
         boost::multi_index::ordered_non_unique<boost::multi_index::tag<block_clock_index>,
                                                boost::multi_index::member<message_info, uint32_t, &message_info::block_clock_when_received>>>> message_cache_container;

    message_cache_container _message_cache;
    uint32_t block_clock = 0;

  public:
    /// Sizes the message hash index for @p expected_size items up front, so it does not rehash while the cache fills.
    explicit blockchain_tied_message_cache(size_t expected_size = 0)
    {
      _message_cache.template get<message_hash_index>().reserve(expected_size);
    }

    void block_accepted();
    void cache_message(const smart_ptr_type& item, const message_propagation_data& propagation_data);
    /// caches @p item as if received now, so it stays for another cache duration; one already cached keeps its propagation data
    void refresh_message(const smart_ptr_type& item, const message_propagation_data& propagation_data);
    smart_ptr_type get_item(const message_hash_type& message_hash) const;
    bool contains_item(const message_hash_type& message_hash) const;
    smart_ptr_type get_item_by_contents_hash(const message_hash_type& message_hash) const;
    bool contains_item_by_contents_hash(const message_hash_type& message_hash) const;
    message_propagation_data get_message_propagation_data(const fc::uint160_t& message_contents_hash) const;
    size_t size() const { return _message_cache.size(); }
  };

  template <typename smart_ptr_type>
  void blockchain_tied_message_cache<smart_ptr_type>::block_accepted()
  {
    ++block_clock;
    if (block_clock > cache_duration_in_blocks)
    {
      auto& idx = _message_cache.template get<block_clock_index>();
      idx.erase(idx.begin(), idx.lower_bound(block_clock - cache_duration_in_blocks));
    }
  }

  template <typename smart_ptr_type>
  void blockchain_tied_message_cache<smart_ptr_type>::cache_message(const smart_ptr_type& item, const message_propagation_data& propagation_data)
  {
    _message_cache.insert(message_info(item, block_clock, propagation_data));
  }

  template <typename smart_ptr_type>
  void blockchain_tied_message_cache<smart_ptr_type>::refresh_message(const smart_ptr_type& item, const message_propagation_data& propagation_data)
  {
    auto& idx = _message_cache.template get<message_hash_index>();
    if (const auto iter = idx.find(get_message_hash().get_message_hash_from_item(item)); iter != idx.end())
      idx.modify(iter, [this](message_info& info) { info.block_clock_when_received = block_clock; });
    else
      _message_cache.insert(message_info(item, block_clock, propagation_data));
  }

  template <typename smart_ptr_type>
  smart_ptr_type blockchain_tied_message_cache<smart_ptr_type>::get_item(const message_hash_type& message_hash) const
  {
    const auto& idx = _message_cache.template get<message_hash_index>();
    if (const auto iter = idx.find(message_hash); iter != idx.end())
      return iter->item;
    FC_THROW_EXCEPTION(fc::key_not_found_exception, "Requested message not in cache");
  }

  template <typename smart_ptr_type>
  bool blockchain_tied_message_cache<smart_ptr_type>::contains_item(const message_hash_type& message_hash) const
  {
    const auto& idx = _message_cache.template get<message_hash_index>();
    return idx.find(message_hash) != idx.end();
  }

  template <typename smart_ptr_type>
  smart_ptr_type blockchain_tied_message_cache<smart_ptr_type>::get_item_by_contents_hash(const message_hash_type& message_contents_hash) const
  {
    const auto& idx = _message_cache.template get<message_contents_hash_index>();
    if (const auto iter = idx.find(message_contents_hash); iter != idx.end())
      return iter->item;
    FC_THROW_EXCEPTION(fc::key_not_found_exception, "Requested message not in cache");
  }

  template <typename smart_ptr_type>
  bool blockchain_tied_message_cache<smart_ptr_type>::contains_item_by_contents_hash(const message_hash_type& message_contents_hash) const
  {
    const auto& idx = _message_cache.template get<message_contents_hash_index>();
    return idx.find(message_contents_hash) != idx.end();
  }

  template <typename smart_ptr_type>
  message_propagation_data blockchain_tied_message_cache<smart_ptr_type>::get_message_propagation_data(const fc::uint160_t& hash_of_message_contents_to_lookup) const
  {
    const auto& idx = _message_cache.template get<message_contents_hash_index>();
    if (hash_of_message_contents_to_lookup != fc::uint160_t())
    {
      if (const auto iter = idx.find(hash_of_message_contents_to_lookup); iter != idx.end())
        return iter->propagation_data;
    }
    FC_THROW_EXCEPTION(fc::key_not_found_exception, "Requested message not in cache");
  }

} } // graphene::net
