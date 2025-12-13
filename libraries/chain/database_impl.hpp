#pragma once

#include <hive/chain/database.hpp>
#include <hive/chain/evaluator_registry.hpp>
#include <hive/chain/rc/rc_utility.hpp>
#include <hive/chain/util/decoded_types_data_storage.hpp>

#include <hive/protocol/types.hpp>

#include <hive/utilities/signal.hpp>

#include <atomic>
#include <map>
#include <memory>

namespace hive { namespace chain {

/**
 * Private implementation class for database (pimpl pattern).
 * This header is only for internal use by database*.cpp files.
 */
class database_impl
{
  public:
    database_impl( database& self );
    void register_new_type(util::abstract_type_registrar&);
    void delete_decoded_types_data_storage();
    void create_new_decoded_types_data_storage() { _decoded_types_data_storage = std::make_unique<util::decoded_types_data_storage>(); }

    database&                                         _self;
    evaluator_registry< operation >                   _evaluator_registry;
    resource_credits                                  _rc;
    std::map<account_name_type, block_id_type>        _last_fast_approved_block_by_witness;
    std::unique_ptr<util::decoded_types_data_storage> _decoded_types_data_storage;

    // these used for the node_status API, which reads these values from another thread
    // they're only used to determine if the node is in sync, and nothing particulary bad
    // will happen if we get mismatched values
    std::atomic<uint32_t>                             _last_pushed_block_number = {0};
    std::atomic<uint32_t>                             _last_pushed_block_time = {0}; // the value from a time_point_sec

    // Signals for plugin notifications
    fc::signal<void(const operation_notification&)>       _pre_apply_operation_signal;
    /**
      *  This signal is emitted for plugins to process every operation after it has been fully applied.
      */
    fc::signal<void(const operation_notification&)>       _post_apply_operation_signal;

    fc::signal<void(const custom_operation_notification&)> _pre_apply_custom_operation_signal;
    fc::signal<void(const custom_operation_notification&)> _post_apply_custom_operation_signal;

    /**
      *  This signal is emitted when we start processing a block.
      *
      *  You may not yield from this callback because the blockchain is holding
      *  the write lock and may be in an "inconstant state" until after it is
      *  released.
      */
    fc::signal<void(const block_notification&)>           _pre_apply_block_signal;

    fc::signal<void(uint32_t)>                            _on_irreversible_block;

    fc::signal<void(uint32_t)>                            _switch_fork_signal;

    /**
      *  This signal is emitted after all operations and virtual operation for a
      *  block have been applied but before the get_applied_operations() are cleared.
      *
      *  You may not yield from this callback because the blockchain is holding
      *  the write lock and may be in an "inconstant state" until after it is
      *  released.
      */
    fc::signal<void(const block_notification&)>           _post_apply_block_signal;

    /**
      *  This signal is emitted when any problems occured during block processing
      */
    fc::signal<void(const block_notification&)>           _fail_apply_block_signal;

    /**
      * This signal is emitted any time a new transaction is about to be applied
      * to the chain state.
      */
    fc::signal<void(const transaction_notification&)>     _pre_apply_transaction_signal;

    /**
      * This signal is emitted any time a new transaction has been applied to the
      * chain state.
      */
    fc::signal<void(const transaction_notification&)>     _post_apply_transaction_signal;

    /**
      * Emitted when reindexing starts
      */
    fc::signal<void(const reindex_notification&)>         _pre_reindex_signal;

    /**
      * Emitted when reindexing finishes
      */
    fc::signal<void(const reindex_notification&)>         _post_reindex_signal;

    fc::signal<void(const database&, const database::abstract_index_cntr_t&)> _prepare_snapshot_signal;

    /// Emitted by snapshot plugin implementation to allow all registered plugins to include theirs custom-stored data in the snapshot
    fc::signal<void(const prepare_snapshot_supplement_notification&)> _prepare_snapshot_supplement_signal;
    /// Emitted by snapshot plugin implementation to allow all registered plugins to load theirs custom-stored data from the snapshot
    fc::signal<void(const load_snapshot_supplement_notification&)> _load_snapshot_supplement_signal;

    /**
      * Internal signal to execute deferred registration of plugin indexes.
      */
    fc::signal<void()>                                    _plugin_index_signal;

    /// Emitted when rewards for author and curators are paid out.
    fc::signal<void(const comment_reward_notification&)>  _comment_reward_signal;

    fc::signal<void()>                                    _end_of_syncing_signal;

    /**
      *  This signal is emitted when pushing block is completely finished
      */
    fc::signal<void(const block_notification&)>           _finish_push_block_signal;

    /**
      *  This signal is emitted when all storages have to be wiped
      */
    fc::signal<void()>                                    _wipe_signal;

    /**
      *  This signal is emitted when storages have to be flushed
      */
    fc::signal<void()>                                    _flush_signal;
};

} } // hive::chain
