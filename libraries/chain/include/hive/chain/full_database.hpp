#pragma once

#include <hive/chain/database.hpp>

namespace hive { namespace chain {

  class full_database : public database
  {
      block_log _block_log;

      /**
        * Emitted when reindexing starts
        */
      fc::signal<void(const reindex_notification&)>         _pre_reindex_signal;

      /**
        * Emitted when reindexing finishes
        */
      fc::signal<void(const reindex_notification&)>         _post_reindex_signal;

    public:
      boost::signals2::connection add_pre_reindex_handler               ( const reindex_handler_t&                   func, const abstract_plugin& plugin, int32_t group = -1 );
      boost::signals2::connection add_post_reindex_handler              ( const reindex_handler_t&                   func, const abstract_plugin& plugin, int32_t group = -1 );

      virtual void state_dependent_open( const open_args& args ) override;

    private:
      bool is_included_block_unlocked(const block_id_type& block_id);
      uint32_t reindex_internal( const open_args& args, const std::shared_ptr<full_block_type>& start_block );
    public:
      std::vector<block_id_type> get_blockchain_synopsis(const block_id_type& reference_point, uint32_t number_of_blocks_after_reference_point);
      std::deque<block_id_type>::const_iterator find_first_item_not_in_blockchain(const std::deque<block_id_type>& item_hashes_received); //by is_known_block_unlocked
      std::vector<block_id_type> get_block_ids(const std::vector<block_id_type>& blockchain_synopsis, uint32_t& remaining_item_count, uint32_t limit);

      /**
        * @brief Check if replaying was finished and all blocks from `block_log` were processed.
        *
        * This method is called from a chain plugin, if returns `true` then a synchronization is allowed.
        * If returns `false`, then opening a node should be forbidden.
        *
        * There are output-type arguments: `head_block_num_origin`, `head_block_num_state` for information purposes only.
        *
        * @return information if replaying was finished
        */
      bool is_reindex_complete( uint64_t* head_block_num_origin, uint64_t* head_block_num_state ) const;

      /**
        * @brief Rebuild object graph from block history and open detabase
        *
        * This method may be called after or instead of @ref database::open, and will rebuild the object graph by
        * replaying blockchain history. When this method exits successfully, the database will be open.
        *
        * @return the last replayed block number.
        */
      uint32_t reindex( const open_args& args );
      virtual void close_chainbase(bool rewind) override;

      /**
        *  @return true if the block is in our fork DB or saved to disk as
        *  part of the official chain, otherwise return false
        */
      bool is_known_block( const block_id_type& id )const;
    private:
      bool is_known_block_unlocked(const block_id_type& id)const;
    public:
      block_id_type              find_block_id_for_num( uint32_t block_num )const;
      block_id_type              get_block_id_for_num( uint32_t block_num )const;
      std::shared_ptr<full_block_type> fetch_block_by_id(const block_id_type& id)const;
      std::shared_ptr<full_block_type> fetch_block_by_number( uint32_t num, fc::microseconds wait_for_microseconds = fc::microseconds() )const;
      std::vector<std::shared_ptr<full_block_type>>  fetch_block_range( const uint32_t starting_block_num, const uint32_t count, fc::microseconds wait_for_microseconds = fc::microseconds() );

    private:
      virtual void migrate_irreversible_state_perform(uint32_t old_last_irreversible) override;
      void migrate_irreversible_state_to_blocklog(uint32_t old_last_irreversible);

      void open_block_log(const open_args& args);
      void state_independent_open( const open_args& args ) override;
  };

}}
