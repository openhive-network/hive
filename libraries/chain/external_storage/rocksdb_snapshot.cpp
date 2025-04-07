#include <hive/chain/external_storage/rocksdb_snapshot.hpp>

#include <hive/chain/external_storage/state_snapshot_provider.hpp>

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <rocksdb/utilities/backup_engine.h>
#include <rocksdb/utilities/write_batch_with_index.h>

#include <boost/filesystem.hpp>

namespace hive { namespace chain {

namespace bfs = boost::filesystem;

#define checkStatus(s) FC_ASSERT((s).ok(), "Data access failed: ${m}", ("m", (s).ToString()))

rocksdb_snapshot::rocksdb_snapshot( std::string name, std::string storage_name, const hive::chain::abstract_plugin& plugin, chain::database& db, const bfs::path& storage_path, const external_ah_storage_provider::ptr& provider )
                          : _name( name ), _storage_name( storage_name ), _mainDb( db ), _plugin( plugin ), _storagePath( storage_path ), _provider( provider )
{
  FC_ASSERT( _provider );
}

void rocksdb_snapshot::supplement_snapshot( const hive::chain::prepare_snapshot_supplement_notification& note )
{
  fc::path actual_path(note.external_data_storage_base_path);
  actual_path /= _storage_name;

  if(bfs::exists(actual_path) == false)
    bfs::create_directories(actual_path);

  auto pathString = actual_path.to_native_ansi_path();

  ::rocksdb::Env* backupEnv = ::rocksdb::Env::Default();
  ::rocksdb::BackupEngineOptions backupableDbOptions(pathString);

  std::unique_ptr<::rocksdb::BackupEngine> backupEngine;
  ::rocksdb::BackupEngine* _backupEngine = nullptr;
  auto status = ::rocksdb::BackupEngine::Open(backupEnv, backupableDbOptions, &_backupEngine);

  checkStatus(status);

  backupEngine.reset(_backupEngine);

  ilog("Attempting to create an ${_name} backup in the location: `${p}'", (_name)("p", pathString));

  std::string meta_data = _name + " plugin data. Current head block: ";
  meta_data += std::to_string(_mainDb.head_block_num());

  status = _backupEngine->CreateNewBackupWithMetadata( _provider->getStorage().get(), meta_data, true);
  checkStatus(status);

  std::vector<::rocksdb::BackupInfo> backupInfos;
  _backupEngine->GetBackupInfo(&backupInfos);

  FC_ASSERT(backupInfos.size() == 1);

  note.dump_helper.store_external_data_info(_plugin, actual_path);
}

void rocksdb_snapshot::load_additional_data_from_snapshot( const hive::chain::load_snapshot_supplement_notification& note )
{
  fc::path extdata_path;
  if(note.load_helper.load_external_data_info(_plugin, &extdata_path) == false)
  {
    wlog("No external data present for ${_name} plugin...", (_name));
    return;
  }

  auto pathString = extdata_path.to_native_ansi_path();

  ilog("Attempting to load external data for ${_name} plugin from location: `${p}'", (_name)("p", pathString));

  ::rocksdb::Env* backupEnv = ::rocksdb::Env::Default();
  ::rocksdb::BackupEngineOptions backupableDbOptions(pathString);

  std::unique_ptr<::rocksdb::BackupEngineReadOnly> backupEngine;
  ::rocksdb::BackupEngineReadOnly* _backupEngine = nullptr;
  auto status = ::rocksdb::BackupEngineReadOnly::Open(backupEnv, backupableDbOptions, &_backupEngine);

  checkStatus(status);

  backupEngine.reset(_backupEngine);

  ilog("Attempting to restore an ${_name} backup from the backup location: `${p}'", (_name)("p", pathString));

  _provider->shutdownDb( true );

  ilog("Starting restore of AccountHistoryRocksDB backup into storage location: ${p}.", ("p", _storagePath.string()));

  bfs::path walDir(_storagePath);
  walDir /= "WAL";
  status = backupEngine->RestoreDBFromLatestBackup(_storagePath.string(), walDir.string());
  checkStatus(status);

  ilog("Restoring AccountHistoryRocksDB backup from the location: `${p}' finished", ("p", pathString));

  _provider->openDb( false );
}

}}
