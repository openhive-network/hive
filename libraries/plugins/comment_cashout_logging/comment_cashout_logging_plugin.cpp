#include <hive/plugins/comment_cashout_logging/comment_cashout_logging_plugin.hpp>

#include <hive/chain/util/impacted.hpp>
#include <hive/protocol/config.hpp>

#include <hive/utilities/signal.hpp>

#include <fc/io/json.hpp>
#include <fc/smart_ref_impl.hpp>
#include <boost/format.hpp>


namespace hive { namespace plugins { namespace comment_cashout_logging {

using namespace hive::protocol;

using chain::database;
using chain::operation_notification;

namespace detail {

const uint64_t BLOCK_INTERVAL = 1000000;

class comment_cashout_logging_plugin_impl
{
   public:
      comment_cashout_logging_plugin_impl(const std::string &path, appbase::application& app ) : _db(app.get_plugin<hive::plugins::chain::chain_plugin>().db()),
        _log_dir_path(path) 
      {
      }

      virtual ~comment_cashout_logging_plugin_impl() 
      {
        if (_log_file.is_open())
        {
          _log_file.close();
        }
      }

      void on_pre_apply_operation( const operation_notification& note );
      std::string make_file_name(bool &changed);

      database&                        _db;
      boost::signals2::connection      _pre_apply_operation_conn;

      optional<uint64_t> _starting_block;
      optional<uint64_t> _ending_block;

      std::string _log_dir_path;
      std::ofstream _log_file;

      uint64_t _last_starting_block = 0 * BLOCK_INTERVAL;
      uint64_t _last_ending_block = 1 * BLOCK_INTERVAL;

};

std::string comment_cashout_logging_plugin_impl::make_file_name(bool &changed)
{
  changed = false;
  const uint64_t block_no = _db.head_block_num();

  const uint64_t i = block_no / BLOCK_INTERVAL;
  const uint64_t last_starting_block = i * BLOCK_INTERVAL;
  const uint64_t last_ending_block = (i + 1) * BLOCK_INTERVAL;
  if (last_starting_block != _last_starting_block && last_ending_block != _last_ending_block)
  {
    changed = true;
    _last_starting_block = last_starting_block;
    _last_ending_block = last_ending_block;
  }
  return boost::str(boost::format("%s/%s-%s.log") % _log_dir_path % _last_starting_block % _last_ending_block);
}

std::string asset_num_to_string(uint32_t asset_num)
{
  switch( asset_num)
  {
#ifdef IS_TEST_NET
    case HIVE_ASSET_NUM_HIVE:
      return "TESTS";
    case HIVE_ASSET_NUM_HBD:
      return "TBD";
#else
    case HIVE_ASSET_NUM_HIVE:
      return "HIVE";
    case HIVE_ASSET_NUM_HBD:
      return "HBD";
#endif
    case HIVE_ASSET_NUM_VESTS:
      return "VESTS";
    default:
      return "UNKN";
  }
}

int64_t precision(const asset_symbol_type& symbol)
{
  static int64_t table[] = {
    1, 10, 100, 1000, 10000,
    100000, 1000000, 10000000, 100000000ll,
    1000000000ll, 10000000000ll,
    100000000000ll, 1000000000000ll,
    10000000000000ll, 100000000000000ll
  };
  uint8_t d = symbol.decimals();
  return table[ d ];
}

std::string asset_to_string(asset a)
{
  int64_t prec = precision(a.symbol);
  string result = fc::to_string(a.amount.value / prec);
  if (prec > 1)
  {
    auto fract = a.amount.value % prec;
    result += "." + fc::to_string(prec + fract).erase(0,1);
  }
  return result + " " + asset_num_to_string(a.symbol.asset_num);
}

struct operation_visitor
{
  operation_visitor( database& db, std::ofstream &log_file) :_db(db), _log_file(log_file) {}

  typedef void result_type;

  database& _db;
  std::ofstream& _log_file;

  template<typename T>
  void operator()(T&& v) const
  {
  }

  void operator()(const claim_reward_balance_operation& op) const
  {
    _log_file << "claim_reward_balance_operation" << ";"
       << _db.head_block_num() << ";" 
       << static_cast<std::string>(op.account) << ";" 
       << asset_to_string(op.reward_hive) << ";" 
       << asset_to_string(op.reward_hbd) << ";" 
       << asset_to_string(op.reward_vests) << "\n";
  }

  void operator()(const author_reward_operation &op) const
  {
    _log_file << "author_reward_operation" << ";" 
      << _db.head_block_num() << ";" 
      << static_cast<std::string>(op.author) << ";"
      << op.permlink << ";" 
      << asset_to_string(op.hbd_payout) << ";" 
      << asset_to_string(op.hive_payout) << ";" 
      << asset_to_string(op.vesting_payout)
      << "\n";
  }

  void operator()(const curation_reward_operation& op) const
  {
    _log_file << "curation_reward_operation" << ";" 
      << _db.head_block_num() << ";"
      << static_cast<std::string>(op.curator) << ";"
      << asset_to_string(op.reward) << ";"
      << static_cast<std::string>(op.author) << ";"
      << op.permlink
      << "\n";
  }

  void operator()(const comment_reward_operation& op) const
  {
    _log_file << "comment_reward_operation" << ";" 
      << _db.head_block_num() << ";" 
      << static_cast<std::string>(op.author) << ";" 
      << op.permlink << ";"
      << asset_to_string(op.payout)
      << "\n";
  }

  void operator()(const comment_benefactor_reward_operation& op) const
  {
    _log_file << "comment_benefactor_reward_operation" << ";" 
      << _db.head_block_num() << ";" 
      << static_cast<std::string>(op.benefactor) << ";" 
      << static_cast<std::string>(op.author) << ";" 
      << op.permlink << ";"
      << asset_to_string(op.hbd_payout) << ";" 
      << asset_to_string(op.hive_payout) << ";" 
      << asset_to_string(op.vesting_payout)
      << "\n";
  }
};

void comment_cashout_logging_plugin_impl::on_pre_apply_operation(const operation_notification& note)
{
  
  const uint64_t block_no = _db.head_block_num();
  
  if (!_log_file.is_open())
  {
    bool changed;
    const std::string file_name = make_file_name(changed);
    _log_file.open(file_name);
  }
  else
  {
    bool changed;
    const std::string file_name = make_file_name(changed);
    if (changed)
    {
      _log_file.close();
      _log_file.open(file_name);
    }
  }
  
  if (_starting_block.valid() && _ending_block.valid())
  {
    if (block_no >= *_starting_block && block_no <= *_ending_block)
    {
      note.op.visit(operation_visitor(_db, _log_file));
    }
  }
  else if (_starting_block.valid() && !_ending_block.valid())
  {
    if (block_no >= *_starting_block)
    {
      note.op.visit(operation_visitor(_db, _log_file));
    }
  }
  else if (!_starting_block.valid() && _ending_block.valid())
  {
    if (block_no <= *_ending_block)
    {
      note.op.visit(operation_visitor(_db, _log_file));
    }
  }
  else
  {
    note.op.visit(operation_visitor(_db, _log_file));
  }
}

} // detail

comment_cashout_logging_plugin::comment_cashout_logging_plugin() {}
comment_cashout_logging_plugin::~comment_cashout_logging_plugin() {}

void comment_cashout_logging_plugin::set_program_options(options_description& cli, options_description& cfg)
{
  cfg.add_options()
    ("cashout-logging-starting-block", boost::program_options::value<uint64_t>(), "Starting block for comment cashout log")
    ("cashout-logging-ending-block", boost::program_options::value<uint64_t>(), "Ending block for comment cashout log")
    ("cashout-logging-log-path-dir", boost::program_options::value<boost::filesystem::path>(), "Path to log file")
    ;
}

void comment_cashout_logging_plugin::plugin_initialize( const boost::program_options::variables_map& options )
{
  ilog("Initializing comment cashout plugin");
  if (options.count("cashout-logging-log-path-dir"))
  {
    boost::filesystem::path pth = options["cashout-logging-log-path-dir"].as<boost::filesystem::path>();
    if (!boost::filesystem::exists(pth))
    {
      throw std::runtime_error("Directory does not exists");
    }
    if (!boost::filesystem::is_directory(pth))
    {
      throw std::runtime_error("Path is not directory");
    }
    my = std::make_unique<detail::comment_cashout_logging_plugin_impl>(pth.string(), get_app());
  }
  else
  {
    my = std::make_unique<detail::comment_cashout_logging_plugin_impl>("./", get_app());
  }

  my->_pre_apply_operation_conn = my->_db.add_pre_apply_operation_handler(
    [&]( const operation_notification& note ){ my->on_pre_apply_operation(note); }, *this, 0 );

  if (options.count("cashout-logging-starting-block"))
  {
    my->_starting_block = options["cashout-logging-starting-block"].as<uint64_t>();
  }

  if (options.count("cashout-logging-ending-block"))
  {
    my->_starting_block = options["cashout-logging-ending-block"].as<uint64_t>();
  }
}

void comment_cashout_logging_plugin::plugin_startup() {}

void comment_cashout_logging_plugin::plugin_shutdown()
{
   hive::utilities::disconnect_signal( my->_pre_apply_operation_conn );
}

} } } // hive::plugins::comment_cashout_logging
