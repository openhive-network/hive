#pragma once

#include <hive/chain/database.hpp>
#include <hive/chain/evaluator_registry.hpp>
#include <hive/chain/util/decoded_types_data_storage.hpp>

#include <hive/protocol/types.hpp>

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
    std::map<account_name_type, block_id_type>        _last_fast_approved_block_by_witness;
    std::unique_ptr<util::decoded_types_data_storage> _decoded_types_data_storage;

    // these used for the node_status API, which reads these values from another thread
    // they're only used to determine if the node is in sync, and nothing particulary bad
    // will happen if we get mismatched values
    std::atomic<uint32_t>                             _last_pushed_block_number = {0};
    std::atomic<uint32_t>                             _last_pushed_block_time = {0}; // the value from a time_point_sec
};

} } // hive::chain
