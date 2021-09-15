#include <hive/plugins/sql_serializer/livesync_data_dumper.h>
#include <hive/plugins/sql_serializer/transaction_controllers.hpp>

#include <hive/chain/database.hpp>

namespace hive{ namespace plugins{ namespace sql_serializer {
  livesync_data_dumper::livesync_data_dumper(
      const std::string& db_url
    , const appbase::abstract_plugin& plugin
    , hive::chain::database& chain_db
    ) {
    auto blocks_callback = [this]( std::string&& _text ){
      _block = std::move( _text );
    };

    auto transactions_callback = [this]( std::string&& _text ){
      _transactions = std::move( _text );
    };

    auto transactions_multisig_callback = [this]( std::string&& _text ){
      _transactions_multisig = std::move( _text );
    };

    auto operations_callback = [this]( std::string&& _text ){
      _operations = std::move( _text );
    };

    transactions_controller = build_own_transaction_controller( db_url, "Livesync dumper" );
    constexpr auto NUMBER_OF_PROCESSORS_THREADS = 4;
    auto execute_push_block = [this](block_num_rendezvous_trigger::BLOCK_NUM _block_num ){
      if ( !_block.empty() ) {
        auto transaction = transactions_controller->openTx();

        std::string block_to_dump = _block + "::hive.blocks";
        std::string transactions_to_dump = "ARRAY[" + _transactions + "]::hive.transactions[]";
        std::string signatures_to_dump = "ARRAY[" + _transactions_multisig + "]::hive.transactions_multisig[]";
        std::string operations_to_dump = "ARRAY[" + _operations + "]::hive.operations[]";
        std::string sql_command = "SELECT hive.push_block(" + block_to_dump + "," + transactions_to_dump + "," + signatures_to_dump + "," + operations_to_dump + ")";

        transaction->exec( sql_command );
        transaction->commit();
      }
      _block.clear(); _transactions.clear(); _transactions_multisig.clear(); _operations.clear();
    };
    auto api_trigger = std::make_shared< block_num_rendezvous_trigger >( NUMBER_OF_PROCESSORS_THREADS, execute_push_block );

    _block_writer = std::make_unique<block_data_container_t_writer>(blocks_callback, "Block data writer", api_trigger);
    _transaction_writer = std::make_unique<transaction_data_container_t_writer>(transactions_callback, "Transaction data writer", api_trigger);
    _transaction_multisig_writer = std::make_unique<transaction_multisig_data_container_t_writer>(transactions_multisig_callback, "Transaction multisig data writer", api_trigger);
    _operation_writer = std::make_unique<operation_data_container_t_writer>(operations_callback, "Operation data writer", api_trigger);

    _on_irreversible_block_conn = chain_db.add_irreversible_block_handler(
      [this]( uint32_t block_num ){ on_irreversible_block( block_num ); }
      , plugin
    );

    _on_switch_fork_conn = chain_db.add_switch_fork_handler(
      [this]( uint32_t block_num ){ on_switch_fork( block_num ); }
      , plugin
    );

  }

  livesync_data_dumper::~livesync_data_dumper() {
    _on_irreversible_block_conn.disconnect();
    _on_switch_fork_conn.disconnect();
    livesync_data_dumper::join();
  }

  void livesync_data_dumper::trigger_data_flush( cached_data_t& cached_data, int last_block_num ) {
    _block_writer->trigger( std::move( cached_data.blocks ), false, last_block_num );
    _operation_writer->trigger( std::move( cached_data.operations ), false, last_block_num );
    _transaction_writer->trigger( std::move( cached_data.transactions ), false, last_block_num);
    _transaction_multisig_writer->trigger( std::move( cached_data.transactions_multisig ), false, last_block_num );

    _block_writer->complete_data_processing();
    _operation_writer->complete_data_processing();
    _transaction_writer->complete_data_processing();
    _transaction_multisig_writer->complete_data_processing();
  }

  void livesync_data_dumper::join() {
    _block_writer->join();
    _transaction_writer->join();
    _transaction_multisig_writer->join();
    _operation_writer->join();
  }

  void livesync_data_dumper::wait_for_data_processing_finish()
  {
    _block_writer->complete_data_processing();
    _transaction_writer->complete_data_processing();
    _transaction_multisig_writer->complete_data_processing();
    _operation_writer->complete_data_processing();
  }

  void livesync_data_dumper::on_irreversible_block( uint32_t block_num ) {
    auto transaction = transactions_controller->openTx();
    std::string command = "SELECT hive.set_irreversible(" + std::to_string(block_num) + ")";
    transaction->exec( command );
    transaction->commit();
  }

  void livesync_data_dumper::on_switch_fork( uint32_t block_num ) {
    auto transaction = transactions_controller->openTx();
    std::string command = "SELECT hive.back_from_fork(" + std::to_string(block_num) + ")";
    transaction->exec( command );
    transaction->commit();
  }
}}} // namespace hive::plugins::sql_serializer


