
#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/plugins/chain/chain_plugin.hpp>
#include <hive/chain/full_block.hpp>

#include <fc/optional.hpp>
#include <fc/variant_object.hpp>

#include <map>
#include <fstream>

#define HIVE_DEBUG_NODE_PLUGIN_NAME "debug_node"

namespace hive { namespace protocol {
  struct pow2;
  struct signed_block;
} }

namespace hive { namespace chain {
  struct block_notification;
} }

namespace hive { namespace plugins { namespace debug_node {

using namespace appbase;

namespace detail { class debug_node_plugin_impl; }

class debug_node_plugin : public plugin< debug_node_plugin >
{
  public:
    debug_node_plugin();
    virtual ~debug_node_plugin();

    APPBASE_PLUGIN_REQUIRES( (chain::chain_plugin) )

    static const std::string& name() { static std::string name = HIVE_DEBUG_NODE_PLUGIN_NAME; return name; }

    virtual void set_program_options(
      options_description& cli,
      options_description& cfg ) override;
    virtual void plugin_initialize( const variables_map& options ) override;
    virtual void plugin_startup() override;
    virtual void plugin_shutdown() override;

    chain::database& database();

    // creates and pushes internal custom op transaction when there is none to be reused; returns id
    const protocol::transaction_id_type& make_artificial_transaction_for_debug_update();

    template< typename Lambda >
    void debug_update( Lambda&& callback, uint32_t skip = hive::chain::database::skip_nothing,
      fc::optional<protocol::transaction_id_type> transaction_id = fc::optional<protocol::transaction_id_type>() )
    {
      auto apply_callback_to_debug_updates = [&]( const protocol::transaction_id_type& tx_id, bool internal_tx )
      {
        auto it = _debug_updates.find( tx_id );
        if( it == _debug_updates.end() )
          _debug_updates[tx_id] = {{ callback }, internal_tx };
        else
          it->second.callbacks.emplace_back( callback );
      };

      if( transaction_id )
      {
        // in this mode caller should first register all necessary callbacks and then push transaction
        // (if transaction is not pushed or is from the past, the callbacks will never execute)
        apply_callback_to_debug_updates( *transaction_id, false );
      }
      else
      {
        // in this mode callbacks are executed immediately and then again as a result of reapplying
        // artificial transaction (most likely on next block generation)
        const auto& dummy_tx_id = make_artificial_transaction_for_debug_update();
        apply_callback_to_debug_updates( dummy_tx_id, true );
        callback( database() );
      }
    }

    void debug_set_vest_price(
      const hive::protocol::price& new_price,
      fc::optional<protocol::transaction_id_type> transaction_id = fc::optional<protocol::transaction_id_type>() );

    uint32_t debug_generate_blocks(
      fc::optional<fc::ecc::private_key> debug_key,
      uint32_t count,
      uint32_t skip,
      uint32_t miss_blocks,
      bool immediate_generation
      );
    uint32_t debug_generate_blocks_until(
      fc::optional<fc::ecc::private_key> debug_key,
      const fc::time_point_sec& head_block_time,
      bool generate_sparsely,
      uint32_t skip,
      bool immediate_generation
      );

    void set_json_object_stream( const std::string& filename );
    void flush_json_object_stream();

    void save_debug_updates( fc::mutable_variant_object& target );
    void load_debug_updates( const fc::variant_object& target );

    void calculate_modifiers_according_to_new_price(const hive::protocol::price& new_price,
      const hive::protocol::HIVE_asset& total_hive, const hive::protocol::VEST_asset& total_vests,
      hive::protocol::HIVE_asset& hive_modifier, hive::protocol::VEST_asset& vest_modifier ) const;

    bool logging = true;

    bool allow_throw_exception = false;

  private:
    void on_pre_apply_transaction( const hive::chain::transaction_notification& note );
    void on_post_apply_block( const hive::chain::block_notification& note );
    std::map<protocol::public_key_type, fc::ecc::private_key> _private_keys;

    std::shared_ptr< detail::debug_node_plugin_impl > my;

    //std::shared_ptr< std::ofstream > _json_object_stream;
    //boost::signals2::scoped_connection _changed_objects_conn;
    //boost::signals2::scoped_connection _removed_objects_conn;

    std::vector< std::string > _edit_scripts;
    struct debug_update_type
    {
      std::vector< std::function< void( chain::database& ) > > callbacks;
      bool internal_tx = false;
    };
    std::map< protocol::transaction_id_type, debug_update_type > _debug_updates;
};

} } }
