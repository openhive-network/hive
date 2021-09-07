#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/plugins/account_history_rocksdb/account_history_rocksdb_objects.hpp>

#include <hive/plugins/chain/chain_plugin.hpp>

#include <appbase/application.hpp>

#include <functional>
#include <memory>

namespace hive {

namespace plugins { namespace account_history_rocksdb {

namespace bfs = boost::filesystem;



class account_history_rocksdb_plugin final : public appbase::plugin< account_history_rocksdb_plugin >
{
public:
  APPBASE_PLUGIN_REQUIRES((hive::plugins::chain::chain_plugin))

  account_history_rocksdb_plugin();
  virtual ~account_history_rocksdb_plugin();

  static const std::string& name() { static std::string name = "account_history_rocksdb"; return name; }

  virtual void set_program_options(
    boost::program_options::options_description &command_line_options,
    boost::program_options::options_description &config_file_options
    ) override;

  virtual void plugin_initialize(const boost::program_options::variables_map& options) override;
  virtual void plugin_startup() override;
  virtual void plugin_shutdown() override;

  void find_account_history_data(const protocol::account_name_type& name, uint64_t start, uint32_t limit, bool include_reversible,
    std::function<bool(unsigned int, const rocksdb_operation_object&)> processor) const;
  bool find_operation_object(size_t opId, rocksdb_operation_object* data) const;
  void find_operations_by_block(size_t blockNum, bool include_reversible,
    std::function<void(const rocksdb_operation_object&)> processor) const;
  std::pair< uint32_t/*nr last block*/, uint64_t/*operation-id to resume from*/ > enum_operations_from_block_range(
    uint32_t blockRangeBegin, uint32_t blockRangeEnd, bool include_reversible,
    fc::optional<uint64_t> operationBegin, fc::optional<uint32_t> limit,
    std::function<bool(const rocksdb_operation_object&, uint64_t, bool)> processor) const;
  bool find_transaction_info(const protocol::transaction_id_type& trxId, bool include_reversible, uint32_t* blockNo, uint32_t* txInBlock) const;

private:
  class impl;

  std::unique_ptr<impl> _my;
  uint32_t              _blockLimit = 0;
  bool                  _doImmediateImport = false;
};


} } } // hive::plugins::account_history_rocksdb
