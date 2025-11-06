#pragma once

namespace appbase
{
class abstract_plugin;
} /// namespace appbase

namespace fc
{
class path;
} /// namespace fc

namespace hive {
namespace chain {
struct open_args;
} /// namespace chain

namespace plugins {
namespace chain {

using abstract_plugin = appbase::abstract_plugin;

/** Purpose of this interface is provide state snapshot functionality to the chain_plugin even it is implemented in separate one.
*/
class state_snapshot_provider
{
  public:
    /** Allows to process explicit snapshot requests specified at application command line.
    */
    virtual void process_explicit_snapshot_dump_requests(const hive::chain::open_args& openArgs) = 0;
    virtual void process_explicit_snapshot_load_requests(const hive::chain::open_args& openArgs) = 0;

  protected:
    virtual ~state_snapshot_provider() = default;
};

class snapshot_dump_helper
{
public:
  /// <summary>
  /// Allows to store additional (external) data (typically held in plugin) in the snapshot being produced atm.
  /// </summary>
  /// <param name="dataKey">unique name for external data - typically name of the plugin related to that data</param>
  /// <param name="storage_path">specifies the directory where external data shall be written to</param>
  virtual void store_external_data_info( const std::string& dataKey, const fc::path& storage_path ) = 0;

protected:
  virtual ~snapshot_dump_helper() {}
};

class snapshot_load_helper
{
public:
  /// <summary>
  /// Allows to ask snapshot provider for additional data (to be loaded) (typically for a plugin).
  /// </summary>
  /// <param name="dataKey">unique name for external data - typically name of the plugin related to that data</param>
  /// <param name="storage_path">output parameter to be filled with the storage path of external data</param>
  /// <returns>true if data was present</returns>
  virtual bool load_external_data_info( const std::string& dataKey, fc::path* storage_path ) = 0;

protected:
  virtual ~snapshot_load_helper() {}

};

}}}