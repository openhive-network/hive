#pragma once

namespace hive {
namespace chain {
struct open_args;
} /// namespace chain

namespace plugins {
namespace chain {


/** Purpose of this interface is provide state snapshot functionality to the chain_plugin even it is implemented in separate one.
*/
class state_snapshot_provider
{
  public:
    /** Allows to process explicit snapshot requests specified at application command line.
    */
    virtual void process_explicit_snapshot_requests(const hive::chain::open_args& openArgs) = 0;

  protected:
    virtual ~state_snapshot_provider() = default;
};

}}}