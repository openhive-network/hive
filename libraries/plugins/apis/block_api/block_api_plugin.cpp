#include <hive/plugins/block_api/block_api.hpp>
#include <hive/plugins/block_api/block_api_plugin.hpp>

namespace hive { namespace plugins { namespace block_api {

block_api_plugin::block_api_plugin() {}
block_api_plugin::~block_api_plugin() {}

void block_api_plugin::set_program_options(
  options_description& cli,
  options_description& cfg ) {}

void block_api_plugin::plugin_initialize( const variables_map& options )
{
  api = std::make_shared< block_api >();
}

void block_api_plugin::plugin_startup() {}

void block_api_plugin::plugin_shutdown() {}


database& init()
{
  static database db;
  static auto inited = false;
  if(inited)
    return db;
  inited = true;


// my->_db.register_custom_operation_interpreter( my->_custom_operation_interpreter );
//     auto chainId = chainPlugin.db().get_chain_id();
    db.set_flush_interval( 10'000 );//10 000
//     db.add_checkpoints( loaded_checkpoints );// empty flat_map<uint32_t,block_id_type> loaded_checkpoints;
    db.set_require_locking( false );// false 
    const auto& abstract_index_cntr = db.get_abstract_index_cntr();


    hive::chain::open_args db_open_args;
    db_open_args.data_dir = "/home/dev/mainnet-5m/blockchain2"; //"/home/dev/mainnet-5m/blockchain"
    db_open_args.shared_mem_dir = db_open_args.data_dir ; // "/home/dev/mainnet-5m/blockchain"
    db_open_args.initial_supply = HIVE_INIT_SUPPLY; // 0
    db_open_args.hbd_initial_supply = HIVE_HBD_INIT_SUPPLY;// 0

    db_open_args.shared_file_size = 25769803776;  //my->shared_memory_size = fc::parse_size( options.at( "shared-file-size" ).as< string >() );

    db_open_args.shared_file_full_threshold = 0;// 0
    db_open_args.shared_file_scale_rate = 0;// 0
    db_open_args.chainbase_flags = 0;// 0
    db_open_args.do_validate_invariants = false; // false
    db_open_args.stop_replay_at = 0;//0
    db_open_args.exit_after_replay = false;//false
    db_open_args.force_replay = true;// true
    db_open_args.validate_during_replay = false;// false
    db_open_args.benchmark_is_enabled = false;//false
    // db_open_args.database_cfg = fc::variant database_config();// empty fc::variant database_config;
    db_open_args.replay_in_memory = false;// false
    // db_open_args.replay_memory_indices = std::vector< std::string >; // ? empty vector
    db_open_args.enable_block_log_compression = true;// true
    db_open_args.block_log_compression_level = 15;// 15




    db.open( db_open_args );
//         init_schema();
//         initialize_state_independent_data(args);
//             initialize_indexes();
//             initialize_evaluators();
//             initialize_irreversible_storage();
//             with_write_lock([&]()
//             {
//             init_genesis(args.initial_supply, args.hbd_initial_supply);
//                 auth_inhibitor(database& db) : db(db), old_flags(db.node_properties().skip_flags)
//                 modify( get_account( HIVE_INIT_MINER_NAME ), [&]( account_object& a )
//                 { db.node_properties().skip_flags = old_flags; }
//         init_hardforks();
//     load_state_initial_data(args);
//         head_block_num();
//             get_dynamic_global_properties().head_block_number;
//         get_last_irreversible_block_num();
//         with_read_lock([&]() {
//             const auto& hardforks = get_hardfork_property_object();
//                 bool operator <= ( const version& o )const { return _operator< operator_type::less_equal >(     o.v_num, []( uint32_t val ){ return val & 0xFFFF0000; } ); }



// db.reindex( db_open_args );
//     reindex_internal( args, start_block );
//         apply_block(full_block, skip_flags);
//           _apply_block(full_block);


  return db;
}


void consume_json_block_impl(const char *json_block)
{


  static auto stop2 = 1; 
  while(stop2)
  {
     int *a = 0;
     a=a;
  }

  database& db = init();

    fc::variant v = fc::json::from_string( std::string{ json_block } );

    
    


    hive::plugins::block_api::api_signed_block_object sb;



    fc::from_variant( v, sb );

    auto siz = sb.transactions.size();
    siz = siz;

    std::shared_ptr<full_block_type> fb_ptr = full_block_type::create_from_signed_block(sb);



  uint64_t skip_flags = hive::plugins::chain::database::skip_validate_invariants | 
      hive::plugins::chain::database::skip_block_log;
  skip_flags |= hive::plugins::chain::database::skip_witness_signature |
    hive::plugins::chain::database::skip_transaction_signatures |
    hive::plugins::chain::database::skip_transaction_dupe_check |
    hive::plugins::chain::database::skip_tapos_check |
    hive::plugins::chain::database::skip_merkle_check |
    hive::plugins::chain::database::skip_witness_schedule_check |
    hive::plugins::chain::database::skip_authority_check |
    hive::plugins::chain::database::skip_validate;



    db.apply_block(fb_ptr, skip_flags);

    int a = 0;
    a=a;
}



} } } // hive::plugins::block_api
