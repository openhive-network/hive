#include <hive/chain/hive_fwd.hpp>

#include <appbase/application.hpp>

#include <hive/protocol/hive_operations.hpp>
#include <hive/protocol/get_config.hpp>
#include <hive/protocol/transaction_util.hpp>

#include <hive/chain/block_summary_object.hpp>
#include <hive/chain/compound.hpp>
#include <hive/chain/custom_operation_interpreter.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/db_with.hpp>
#include <hive/chain/evaluator_registry.hpp>
#include <hive/chain/global_property_object.hpp>
#include <hive/chain/optional_action_evaluator.hpp>
#include <hive/chain/pending_required_action_object.hpp>
#include <hive/chain/pending_optional_action_object.hpp>
#include <hive/chain/required_action_evaluator.hpp>
#include <hive/chain/smt_objects.hpp>
#include <hive/chain/hive_evaluator.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/transaction_object.hpp>
#include <hive/chain/shared_db_merkle.hpp>
#include <hive/chain/witness_schedule.hpp>
#include <hive/chain/blockchain_worker_thread_pool.hpp>

#include <hive/chain/util/asset.hpp>
#include <hive/chain/util/reward.hpp>
#include <hive/chain/util/uint256.hpp>
#include <hive/chain/util/reward.hpp>
#include <hive/chain/util/manabar.hpp>
#include <hive/chain/util/rd_setup.hpp>
#include <hive/chain/util/nai_generator.hpp>
#include <hive/chain/util/dhf_processor.hpp>
#include <hive/chain/util/delayed_voting.hpp>

#include <fc/smart_ref_impl.hpp>
#include <fc/uint128.hpp>

#include <fc/container/deque.hpp>

#include <fc/io/fstream.hpp>

#include <boost/scope_exit.hpp>
#include <boost/range/adaptor/reversed.hpp>

#include <iostream>

#include <cstdint>
#include <deque>
#include <fstream>
#include <functional>

#include <stdlib.h>


#include <iostream>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <vector>

namespace{
std::string to_hex(const char* data, std::size_t size) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for(std::size_t i = 0; i < size; ++i) {
        ss << std::setw(2) << static_cast<unsigned>(static_cast<unsigned char>(data[i]));
    }
    return ss.str();
}
}

namespace hive { namespace chain {

void database::non_transactional_apply_block(const std::shared_ptr<full_block_type>& full_block, op_iterator_ptr op_it, uint32_t skip)
{

  detail::with_skip_flags( *this, skip, [&]()
  {
    _apply_block(full_block, [this, &op_it](const std::shared_ptr<full_block_type>& full_block, uint32_t skip) 
    {
       _process_operations(std::move(op_it));
    });
    
  } );

}



void database::_process_operations(op_iterator_ptr op_it)
{

  while(op_it->has_next()) 
  {
  //   op_iterator::op_view_t opv  = 
  //   auto [raw_data, data_length] = opv.first;
  //   auto vectorek = opv.second;

  // std::cout << "Inside _process_operations: " << std::endl;
  //  std::cout << "raw_data in hex: \n" << to_hex(raw_data, data_length) << std::endl;
  //   std::cout << "data_length in hex: \n" << std::hex << data_length << std::endl;
  //   std::cout << "vectorek in hex: \n" << to_hex(vectorek.data(), vectorek.size()) << std::endl;


    hive::protocol::operation op = op_it->unpack_from_char_array_and_next();

    //_current_op_in_trx = 0;
    try
    {
      apply_operation(op);
      //++_current_op_in_trx;
    }
    FC_CAPTURE_AND_RETHROW((op))
  }
  // end process the operations
}



}}