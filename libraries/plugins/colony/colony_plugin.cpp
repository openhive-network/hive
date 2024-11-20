
#include <hive/chain/hive_fwd.hpp>

#include <hive/plugins/colony/colony_plugin.hpp>
#include <hive/plugins/p2p/p2p_plugin.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/index.hpp>
#include <hive/chain/account_object.hpp>
#include <hive/chain/comment_object.hpp>

#include <fc/thread/thread.hpp>

#define COLONY_COMMENT_BUFFER 10000 // number of recent comments kept as targets for replies/votes
#define COLONY_MAX_CONCURRENT_TRANSACTIONS 100 // number of transactions per thread that can be sent with no wait
#define COLONY_DEFAULT_THREADS 4 // number of working threads used by default (threads have separate pools of users)
#define COLONY_DEFAULT_MAX_TRANSACTIONS 1000 // number of max transactions per block used by default when there is
  // no other source of default nor explicit value - the value is split between threads
#define COLONY_WAIT_FOR_WORK fc::milliseconds( 50 ) // time to wait before next check if block was made when there
  // is nothing to produce for current block
#define COLONY_OVERFLOW_TOLERANCE_MIN 100 // minimum margin of overflowing transactions
#define COLONY_OVERFLOW_TOLERANCE_MAX 500 // maximum margin of overflowing transactions
#define COLONY_OVERFLOW_TOLERANCE_RATE 500 // percentage rate of allowed overflowing transactions (in BP)

namespace hive { namespace plugins { namespace colony {

using namespace hive::protocol;
using namespace hive::chain;

enum kind_of_operation
{
  ARTICLE, // articles are root comments with associated comment_options with beneficiaries
  REPLY, // replies are non-root comments linking to articles or other replies
  VOTE, // random upvote/downvote for any comment
  TRANSFER, // transfer of 0.001 HBD between two users with memo
  CUSTOM_JSON, // unique id json with single string element
  NUMBER_OF_OPERATIONS
};

struct operation_params
{
  uint32_t min = 0, max = 0; //0 <= min <= max, max has different limit depending on kind of operation
  uint32_t weight = 0; // relative weight of operation kind in whole mix
  double exponent = 1.0; // >= 0; values below 1.0 favor bigger operations, above favor small
    // the size of operation is random with size = (max-min) * rand(0..1)**exponent + min

  void validate( uint32_t max_limit ) const
  {
    FC_ASSERT( min >= 0, "Minimal additional size cannot be negative value." );
    FC_ASSERT( min <= max, "Maximal additional size cannot be smaller than minimal." );
    FC_ASSERT( max <= max_limit, "Maximal additional size has to be at most ${max_limit}.", ( max_limit ) );
    FC_ASSERT( weight >= 0, "Weight cannot be negative value." );
    FC_ASSERT( exponent >= 0.0, "Exponent has to be positive value." );
  }

  int32_t randomize() const
  {
    double x = double( std::rand() ) / double( RAND_MAX );
    FC_ASSERT( 0.0 <= x && x <= 1.0, "Wrong randomized value ${x}", (x) );
    int32_t extra_operation_size = ( max - min ) * std::pow( x, exponent ) + min;
    return extra_operation_size;
  }
};

struct operation_stats
{
  uint64_t count = 0;
  uint64_t extra_size = 0;
  uint64_t real_size = 0;
};

namespace detail {

class colony_plugin_impl;

struct transaction_builder
{
  const colony_plugin_impl&                           _common;
  fc::thread                                          _worker;
  std::vector< const account_object* >                _accounts;
  uint8_t                                             _id;
  decltype( _accounts )::iterator                     _current_account;
  std::array< operation_stats, NUMBER_OF_OPERATIONS > _stats;
  uint32_t                                            _reply_substitutions = 0;
  uint32_t                                            _vote_substitutions = 0;
  uint32_t                                            _transfer_substitutions = 0;
  uint32_t                                            _failed_transactions = 0;
  uint32_t                                            _failed_rc = 0;
  uint32_t                                            _tx_num = 0;
  uint32_t                                            _block_num = 0;
  uint32_t                                            _tx_to_produce = 0;
  uint32_t                                            _concurrent_tx_count = 0;

  hive::protocol::signed_transaction                  _tx;
  std::atomic_bool                                    _tx_needs_update = { true };

  transaction_builder( const colony_plugin_impl& my, uint8_t id )
    : _common( my ), _id( id ), _current_account( _accounts.end() )
  {
    _worker.set_name( "colony_worker_" + std::to_string( _id ) );
  }

  transaction_builder( const transaction_builder& ) = delete;
  transaction_builder( transaction_builder&& ) = delete;
  transaction_builder& operator=( const transaction_builder& ) = delete;
  transaction_builder& operator=( transaction_builder&& ) = delete;

  void print_stats() const;

  void init( int starting_point = 0 ); // first task of worker thread
  bool accept_transaction( full_transaction_ptr full_tx );
  void push_transaction( kind_of_operation kind );
  void build_transaction();
  void fill_string( std::string& str, size_t size );
  void build_article( const account_object& actor, uint64_t nonce );
  void build_reply( const account_object& actor, uint64_t nonce );
  void build_vote( const account_object& actor, uint64_t nonce );
  void build_transfer( const account_object& actor, uint64_t nonce );
  void build_custom( const account_object& actor, uint64_t nonce );
};

class colony_plugin_impl
{
  public:
    colony_plugin_impl( colony_plugin& _plugin, appbase::application& app )
    : _chain( app.get_plugin< hive::plugins::chain::chain_plugin >() ),
      _db( _chain.db() ),
      _self( _plugin ), theApp( app ) {}
    ~colony_plugin_impl() {}

    void start( uint32_t block_num );

    void end_of_sync();
    void post_apply_transaction( const transaction_notification& note );
    void post_apply_block( const block_notification& note );

    chain::chain_plugin&          _chain;
    plugins::p2p::p2p_plugin*     _p2p_ptr = nullptr;
    chain::database&              _db;
    colony_plugin&                _self;
    appbase::application&         theApp;

    boost::signals2::connection   _end_of_sync_conn;
    boost::signals2::connection   _post_apply_transaction_conn;
    boost::signals2::connection   _post_apply_block_conn;

    std::vector< fc::ecc::private_key >                  _sign_with;
    std::array< operation_params, NUMBER_OF_OPERATIONS > _params;
    std::list< transaction_builder >                     _threads;
    uint32_t                                             _total_weight = 0;
    uint32_t                                             _max_tx_per_block = -1;
    uint32_t                                             _dynamic_tx_per_block = -1;
    uint32_t                                             _overflowing_tx = 0;
    uint32_t                                             _start_at_block = 0;
    uint8_t                                              _max_threads = COLONY_DEFAULT_THREADS;
    bool                                                 _disable_broadcast = false;
    bool                                                 _fill_comment_buffers = false;

    typedef std::pair< hive::protocol::account_name_type, std::string > comment_data;
    // comments that are target for other comments and votes are shared between threads
    std::array< comment_data, COLONY_COMMENT_BUFFER >    _comments;
    uint32_t                                             _last_comment = 0;
};

void transaction_builder::print_stats() const
{
  ilog( "Production stats for thread ${t}", ( "t", _worker.name() ) );
  ilog( "Number of transactions: ${t}", ( "t", _tx_num ) );
  if( _tx_num == 0 )
    return;
  auto print_stat = [&]( const operation_stats& stats, const char* name )
  {
    if( stats.count == 0 )
      return;
    ilog( "${d}: ${c} (%${p}.${bp}), avg.extra: ${e}, avg.size: ${s}",
      ( "d", name )( "c", stats.count )( "p", stats.count * 100 / _tx_num )
      ( "bp", stats.count * 10000 / _tx_num % 100 )
      ( "e", ( stats.extra_size + stats.count / 2 ) / stats.count )
      ( "s", ( stats.real_size + stats.count / 2 ) / stats.count ) );
  };
  print_stat( _stats[ ARTICLE ], "Articles" );
  print_stat( _stats[ REPLY ], "Replies" );
  print_stat( _stats[ VOTE ], "Votes" );
  print_stat( _stats[ TRANSFER ], "Transfers" );
  print_stat( _stats[ CUSTOM_JSON ], "Custom jsons" );
  if( _reply_substitutions || _vote_substitutions )
  {
    ilog( "${r} replies and ${v} votes substituted with articles due to lack of proper target comment",
      ( "r", _reply_substitutions )( "v", _vote_substitutions ) );
  }
  if( _transfer_substitutions )
  {
    ilog( "${r} transfers substituted with custom jsons due to lack of funds",
      ( "r", _transfer_substitutions ) );
  }
  if( _failed_transactions )
  {
    ilog( "${f} transactions failed with exception (including ${r} due to lack of RC)",
      ( "f", _failed_transactions )( "r", _failed_rc ) );
  }
}

void transaction_builder::init( int starting_point )
{
  _current_account = _accounts.begin();
  std::advance( _current_account, starting_point % _accounts.size() );
  _worker.async( [this]()
  {
    std::srand( std::time( 0 ) + _id );
    ilog( "Starting thread ${n} with ${w} workers.",
      ( "n", _worker.name() )( "w", _accounts.size() ) );
    build_transaction();
  } );
}

bool transaction_builder::accept_transaction( full_transaction_ptr full_tx )
{
  bool result = false;
  try
  {
    ++_concurrent_tx_count;
    _common._chain.accept_transaction( full_tx, chain::chain_plugin::lock_type::fc );
    if( _common._p2p_ptr )
      _common._p2p_ptr->broadcast_transaction( full_tx );
    --_concurrent_tx_count;
    result = true;
  }
  catch( const fc::canceled_exception& ex )
  {
    // interrupt request - nothing to do
  }
  catch( const not_enough_rc_exception& ex )
  {
    // elog( "Shortfall of RC during accepting transaction: ${d} transaction: ${t}",
    //  ( "d", ex.to_string() )( "t", full_tx->get_transaction() ) );
    //considering that colony is mostly made to stress test node with large amounts of
    //transactions in max sized blocks, getting to the point where RC is missing is
    //actually expected and normal, so logging it would be just spam
    ++_failed_rc;
    ++_failed_transactions;
  }
  catch( const fc::exception& ex )
  {
    //depending on amount of colony workers and transaction frequency parameters,
    //especially votes which are hard to make unique, one of the way transaction can
    //fail (but has no specialized exception) is duplicate transaction check; it can
    //happen in course of normal work, but when RC on workers starts to end, worker
    //rotation increases making it more likely to run into duplicates;
    //for similar reasons witness limit on custom ops might be another "typical"
    //exception that might happen with too few workers and increases in frequency with
    //lack of RC;
    //last one that would be nice to catch is boost::interprocess::bad_alloc;
    //it has separate exception class, but before it could be caught here, it is translated to
    //generic fc::exception in one of many FC_CAPTURE_AND_RETHROW calls; too bad, because
    //it would be nice if it triggered app.kill(), because at that point all you get is log spam
    elog( "Exception during accepting transaction: ${d} transaction: ${t}",
      ( "d", ex.to_string() )( "t", full_tx->get_transaction() ) );
    ++_failed_transactions;
  }
  catch( ... )
  {
    elog( "Unknown exception during accepting transaction ${t}", ( "t", full_tx->get_transaction() ) );
    ++_failed_transactions;
  }
  return result;
}

void transaction_builder::push_transaction( kind_of_operation kind )
{
  full_transaction_ptr full_tx = full_transaction_type::create_from_signed_transaction( _tx, serialization_type::hf26, false );
  _tx.clear();
  full_tx->sign_transaction( _common._sign_with, _common._db.get_chain_id(), serialization_type::hf26, true );
  _stats[ kind ].real_size += full_tx->get_transaction_size();
  if( _concurrent_tx_count >= COLONY_MAX_CONCURRENT_TRANSACTIONS )
  {
    // when there is too many concurrent transactions next one becomes blocking which should
    // act as "cool down" and let other threads (like witness) process their requests
    accept_transaction( full_tx );
    // since all previously sent transactions are earlier in queue than this one, once it
    // returns here all transacions should already be processed (taken out of writer queue);
    // _concurrent_tx_count might still be > 0 because, while transactions were already processed,
    // there is no guarantee that respective tasks were already activated
  }
  else
  {
    _worker.async( [ this, full_tx = std::move(full_tx) ]()
    {
      accept_transaction( full_tx );
    } );
  }
}

void transaction_builder::build_transaction()
{
  if( _common.theApp.is_interrupt_request() )
  {
    ilog( "Thread ${t} stopped building new transactions (${p} pending).",
      ( "t", _worker.name() )( "p", _concurrent_tx_count ) );
    _worker.quit();
    return;
  }
  else if( _tx_needs_update.load( std::memory_order_relaxed ) )
  {
    // block was produced since last transaction (or we just started)
    _common._db.with_read_lock( [&]()
    {
      _tx.set_reference_block( _common._db.head_block_id() );
      _tx.set_expiration( _common._db.head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      _block_num = _common._db.head_block_num();
      if( _common._max_tx_per_block == 0 ) // no limit
      {
        _tx_to_produce = -1;
      }
      else
      {
        uint32_t effective_total;
        if( _common._overflowing_tx == 0 ) // transactions didn't overflow - go with given max
          effective_total = _common._max_tx_per_block;
        else // use adjusted rate
          effective_total = std::max( 0l, (int64_t)_common._dynamic_tx_per_block - _common._overflowing_tx );
        _tx_to_produce = ( effective_total + _common._max_threads - 1 ) / _common._max_threads;
        dlog( "Scheduling production of ${x} transactions in ${t}, previous overflow was ${o}",
          ( "x", _tx_to_produce )( "t", _worker.name() )( "o", _common._overflowing_tx ) );
      }
      _tx_needs_update.store( false, std::memory_order_relaxed );
    } );
  }

  if( _tx_to_produce == 0 )
  {
    dlog( "Thread ${t} has nothing to do. Waiting...", ( "t", _worker.name() ) );
    _worker.schedule( [this]() { build_transaction(); }, fc::time_point::now() + COLONY_WAIT_FOR_WORK );
    return;
  }

  uint32_t randomTx = std::rand() % _common._total_weight;
  const auto& account = *(*_current_account);
  ++_current_account;
  if( _current_account == _accounts.end() )
    _current_account = _accounts.begin();
  uint64_t nonce = ( (uint64_t)_block_num << 40 ) | ( (uint64_t)_tx_num << 8 ) | (uint64_t)_id;
    // nonce is composed of _id for uniqueness between threads, _tx_num for uniqueness between
    // transactions of the same thread within production session and _block_num for uniqueness
    // between different production sessions
  ++_tx_num;
  --_tx_to_produce;
  if( ( _tx_num % 1000000 ) == 0 )
    ilog( "Thread ${t} building its ${n}th transaction", ( "t", _worker.name() )( "n", _tx_num ) );

  do
  try {
    uint32_t max = _common._params[ ARTICLE ].weight;
    if( randomTx < max )
    {
      build_article( account, nonce );
      break;
    }
    max += _common._params[ REPLY ].weight;
    if( randomTx < max )
    {
      build_reply( account, nonce );
      break;
    }
    max += _common._params[ VOTE ].weight;
    if( randomTx < max )
    {
      build_vote( account, nonce );
      break;
    }
    max += _common._params[ TRANSFER ].weight;
    if( randomTx < max )
    {
      build_transfer( account, nonce );
      break;
    }
    // custom_json is default
    {
      build_custom( account, nonce );
      break;
    }
  }
  catch( const fc::exception& ex )
  {
    elog( "Exception during building transaction: ${d} transaction: ${t}",
      ( "d", ex.to_string() )( "t", _tx ) );
    _common.theApp.kill();
  }
  catch( ... )
  {
    elog( "Unknown exception during building transaction ${t}", ( "t", _tx ) );
    _common.theApp.kill();
  }
  while( false );

  _worker.async( [this]() { build_transaction(); } );
}

void transaction_builder::fill_string( std::string& str, size_t size )
{
  str.resize( size, ' ' );
  for( size_t i = 0; i < str.size(); ++i )
  {
    if( i % 13 == 0 )
      continue;
    else
      str.at( i ) = 'a' + ( i % 25 );
  }
}

void transaction_builder::build_article( const account_object& actor, uint64_t nonce )
{
  ++_stats[ ARTICLE ].count;

  comment_operation article;
  article.title = std::to_string( nonce );
  article.parent_permlink = "category" + std::to_string( nonce % 101 );
  article.author = actor.get_name();
  article.permlink = "a" + article.title;
  auto extra_size = _common._params[ ARTICLE ].randomize();
  _stats[ ARTICLE ].extra_size += extra_size;
  fill_string( article.body, extra_size );
  _tx.operations.emplace_back( article );
  int beneficiaries_count = std::min( std::rand() % HIVE_MAX_COMMENT_BENEFICIARIES, (int)_accounts.size() - 1 );
  if( beneficiaries_count > 0 )
  {
    comment_options_operation options;
    options.author = article.author;
    options.permlink = article.permlink;
    comment_payout_beneficiaries extension;
    auto beneficiaryI = _current_account;
    for( int i = 0; i < beneficiaries_count; ++i )
    {
      extension.beneficiaries.emplace_back( (*beneficiaryI)->get_name(), HIVE_1_PERCENT * 5 );
      ++beneficiaryI;
      if( beneficiaryI == _accounts.end() )
        break; // we are using the fact that accounts are sorted, so we can't wrap around
    }
    options.extensions.emplace( extension );
    _tx.operations.emplace_back( options );
  }
  push_transaction( ARTICLE );
}

void transaction_builder::build_reply( const account_object& actor, uint64_t nonce )
{
  uint32_t random_comment = std::rand() % COLONY_COMMENT_BUFFER;
  if( _common._comments[ random_comment ].first == account_name_type() )
  {
    ++_reply_substitutions;
    build_article( actor, nonce );
    return;
  }

  ++_stats[ REPLY ].count;

  comment_operation reply;
  reply.title = std::to_string( nonce );
  reply.parent_author = _common._comments[ random_comment ].first;
  reply.parent_permlink = _common._comments[ random_comment ].second;
  reply.author = actor.get_name();
  reply.permlink = "r" + reply.title;
  auto extra_size = _common._params[ REPLY ].randomize();
  _stats[ REPLY ].extra_size += extra_size;
  fill_string( reply.body, extra_size );
  _tx.operations.emplace_back( reply );
  push_transaction( REPLY );
}

void transaction_builder::build_vote( const account_object& actor, uint64_t nonce )
{
  uint32_t random_comment = std::rand() % COLONY_COMMENT_BUFFER;
  if( _common._comments[ random_comment ].first == account_name_type() )
  {
    ++_vote_substitutions;
    build_article( actor, nonce );
    return;
  }

  ++_stats[ VOTE ].count;

  vote_operation vote;
  vote.voter = actor.get_name();
  vote.author = _common._comments[ random_comment ].first;
  vote.permlink = _common._comments[ random_comment ].second;
  // there is no other place to use nonce - place it in weight
  nonce >>= 8; // but remove id part for better uniqueness
  vote.weight = ( std::rand() > 0 ? 1 : -1 ) * ( ( nonce % HIVE_100_PERCENT ) + 1 );
  _tx.operations.emplace_back( vote );
  push_transaction( VOTE );
}

void transaction_builder::build_transfer( const account_object& actor, uint64_t nonce )
{
  bool use_hive = false;
  if( actor.get_hbd_balance().amount.value == 0 )
  {
    if( actor.get_balance().amount.value == 0 )
    {
      ++_transfer_substitutions;
      build_custom( actor, nonce );
      return;
    }
    use_hive = true;
  }

  ++_stats[ TRANSFER ].count;

  transfer_operation transfer;
  transfer.from = actor.get_name();
  transfer.to = (*_current_account)->get_name();
  if( use_hive )
    transfer.amount = HIVE_asset( 1 );
  else
    transfer.amount = HBD_asset( 1 );
  transfer.memo = std::to_string( nonce );
  auto extra_size = _common._params[ TRANSFER ].randomize();
  _stats[ TRANSFER ].extra_size += extra_size;
  if( extra_size > (int64_t)transfer.memo.size() + 1 )
  {
    extra_size -= (int64_t)transfer.memo.size() + 1;
    std::string fill;
    fill_string( fill, extra_size );
    transfer.memo = transfer.memo + ":" + fill;
  }
  _tx.operations.emplace_back( transfer );
  push_transaction( TRANSFER );
}

void transaction_builder::build_custom( const account_object& actor, uint64_t nonce )
{
  ++_stats[ CUSTOM_JSON ].count;

  custom_json_operation custom;
  custom.required_auths.emplace( actor.get_name() );
  custom.id = "id" + std::to_string( nonce );
  auto extra_size = _common._params[ CUSTOM_JSON ].randomize();
  _stats[ CUSTOM_JSON ].extra_size += extra_size;
  std::string fill;
  fill_string( fill, extra_size );
  custom.json = "{\"v\":\"" + fill + "\"}";
  _tx.operations.emplace_back( custom );
  push_transaction( CUSTOM_JSON );
}

void colony_plugin_impl::start( uint32_t block_num )
{
  if( _start_at_block == (uint32_t)-1 )
    return;
  // make sure the initialization code won't be run for the second time)
  _start_at_block = -1;
  ilog( "Colony plugin starting at block #${block_num}", ( block_num ) );

  // scan existing accounts to extract those for which you can effectively use _sign_with keys
  flat_set<public_key_type> common_keys;
  for( const auto& key : _sign_with )
    common_keys.insert( key.get_public_key() );

  const auto& accounts = _db.get_index< account_index, by_name >();
  const auto& comments = _db.get_index< comment_cashout_index, by_id >();
  _fill_comment_buffers = _params[ REPLY ].weight > 0 || _params[ VOTE ].weight > 0;
  if( _fill_comment_buffers )
  {
    // get some initial comments to be used as targets for replies/votes
    bool shortfall = comments.size() < COLONY_COMMENT_BUFFER;
    if( shortfall )
    {
      wlog( "Not enough initial comments to act as targets for replies/votes (${s} short). "
        "When nonexistent comment is selected as target, the reply/vote will be replaced with article.",
        ( "s", COLONY_COMMENT_BUFFER - comments.size() ) );
      auto paid_comment_count = _db.get_index< comment_index, by_id >().size() - comments.size();
      if( paid_comment_count > 0 )
      {
        wlog( "Note: there are ${c} additional comments in the state, but they were paid out, so "
          "node has no data on their permlinks.", ( "c", paid_comment_count ) );
      }
    }
    _last_comment = 0;
    for( const auto& comment : comments )
    {
      auto& comment_data = _comments[ _last_comment ];
      comment_data.first = _db.get_account( comment.get_author_id() ).get_name();
      comment_data.second = comment.get_permlink();
      ++_last_comment;
      if( _last_comment >= COLONY_COMMENT_BUFFER )
      {
        _last_comment = 0;
        break;
      }
    }
    std::string initial_comments = std::to_string( _last_comment );
    if( _last_comment == 0 )
    {
      if( _comments[0].first == account_name_type() )
        initial_comments = "no";
      else
        initial_comments = "full buffer (" + std::to_string( COLONY_COMMENT_BUFFER ) + ") of";
    }
    ilog( "Colony threads have ${initial_comments} initial comments to use.", ( initial_comments ) );
  }
  decltype( _threads )::iterator threadI;
  int i = 0;
  int not_matching_accounts = 0;
  fc::time_point_sec lib_ts = HIVE_GENESIS_TIME;
  {
    auto lib = _db.get_last_irreversible_block_data();
    if( lib.get() != nullptr )
      lib_ts = lib->get_block_header().timestamp;
  }
  for( const auto& account : accounts )
  {
    if( account.get_block_creation_time() > lib_ts )
    {
      dlog( "Account ${a} is too fresh to be safely used as colony worker.", ( "a", account.get_name() ) );
      continue;
    }

    auto get_active = [&]( const std::string& name ) { return authority( _db.get< account_authority_object, by_account >( name ).active ); };
    auto get_owner = [&]( const std::string& name ) { return authority( _db.get< account_authority_object, by_account >( name ).owner ); };
    auto get_posting = [&]( const std::string& name ) { return authority( _db.get< account_authority_object, by_account >( name ).posting ); };
    auto get_witness_key = [&]( const std::string& name ) { try { return _db.get_witness( name ).signing_key; } FC_CAPTURE_AND_RETHROW( ( name ) ) };

    required_authorities_type required_authorities;
    required_authorities.required_active.insert( account.get_name() );

    if( hive::protocol::has_authorization( required_authorities, common_keys, get_active, get_owner, get_posting, get_witness_key ) )
    {
      if( i < _max_threads )
        threadI = _threads.emplace( _threads.end(), *this, (uint8_t)i );
      threadI->_accounts.emplace_back( &account );
      ++i;
      ++threadI;
      if( threadI == _threads.end() )
        threadI = _threads.begin();
    }
    else
    {
      dlog( "Active authority of ${a} does not match given set of private keys.", ( "a", account.get_name() ) );
      ++not_matching_accounts; // expected to have at least built-in accounts as not matching
    }
  }

  if( not_matching_accounts > 0 )
  {
    ilog( "Found ${a} accounts, ${f} have active authorities that don't match given set of private keys.",
      ( "a", accounts.size() )( "f", not_matching_accounts ) );
  }
  if( _threads.empty() )
  {
    elog( "No accounts suitable for use as colony workers! Shutting down." );
    theApp.generate_interrupt_request();
  }
  else if( i < _max_threads )
  {
    wlog( "Not enough accounts suitable for use as colony workers (${i}) to share between ${n} threads.", ( i )( "n", _max_threads ) );
    _max_threads = i;
  }

  if( not _disable_broadcast && not theApp.is_interrupt_request() )
  {
    auto* _p2p_plugin = theApp.find_plugin< hive::plugins::p2p::p2p_plugin >();
    if( _p2p_plugin == nullptr )
      wlog( "P2P plugin not present - transactions produced by colony won't be broadcast" );
    else if( _chain.is_p2p_enabled() == false )
      wlog( "P2P plugin disabled - transactions produced by colony won't be broadcast" );
    else
      _p2p_ptr = _p2p_plugin;
  }

  std::srand( std::time( 0 ) );
  for( auto& thread : _threads )
  {
    // randomize starting point to make it easier for multiple colony nodes to run concurrently on the same set of accounts
    thread.init( std::rand() );
  }
}

void colony_plugin_impl::end_of_sync()
{
  uint32_t head_block = _db.head_block_num();
  if( _start_at_block <= head_block )
    start( head_block ); // while not under write lock, we can still call it safely, since it is called from writer thread

  if( not theApp.is_interrupt_request() )
  {
    // only now attach to main signals
    _post_apply_transaction_conn = _db.add_post_apply_transaction_handler( [&]( const transaction_notification& note )
      { post_apply_transaction( note ); }, _self, 0 );
    _post_apply_block_conn = _db.add_post_apply_block_handler( [&]( const block_notification& note )
      { post_apply_block( note ); }, _self, 0 );
  }

  // it won't retrigger anyway, but just in case we should detach from this signal
  chain::util::disconnect_signal( _end_of_sync_conn );
}

void colony_plugin_impl::post_apply_transaction( const transaction_notification& note )
{
  if( _db.is_reapplying_one_tx() )
    ++_overflowing_tx; // the counter will not cover postponed transactions, but it is better than nothing
}

void colony_plugin_impl::post_apply_block( const block_notification& note )
{
  if( _start_at_block <= note.block_num )
  {
    start( note.block_num );
    return;
  }

  // it used to be done once every couple hundred blocks for the purpose of updating
  // expiration/TaPoS, but it turned out colony tends to build too many transactions
  // which leads to overflow of postponed transactions and related problems (f.e.
  // comments that were already sent could turn out not to be visible as targets for
  // replies/votes); now it happens every block, which also allows for limiting how
  // much work is done for each block
  for( auto& thread : _threads )
    thread._tx_needs_update.store( true, std::memory_order_relaxed );
  uint32_t fit_in_block = note.full_block->get_full_transactions().size();
  uint32_t margin = ( (uint64_t)fit_in_block * COLONY_OVERFLOW_TOLERANCE_RATE ) / HIVE_100_PERCENT;
  if( margin < COLONY_OVERFLOW_TOLERANCE_MIN )
    margin = COLONY_OVERFLOW_TOLERANCE_MIN;
  else if( margin > COLONY_OVERFLOW_TOLERANCE_MAX )
    margin = COLONY_OVERFLOW_TOLERANCE_MAX;
  _dynamic_tx_per_block = std::min( fit_in_block + margin, _max_tx_per_block );
  // we can't compute final rate per thread because we don't know yet how many transactions will
  // remain in pending after reapplication
  _overflowing_tx = 0;

  if( _fill_comment_buffers )
  {
    // ABW: it is not fully thread safe, because despite _tx_needs_update being set
    // for each thread, they could still be producing previous transaction and accessing
    // data that we are updating below
    for( const auto& tx : note.full_block->get_full_transactions() )
    {
      const operation& op = tx->get_transaction().operations.front();
      if( op.which() == operation::tag<comment_operation>::value )
      {
        const comment_operation& comment = op.get< comment_operation >();
        // remember comment
        _comments[ _last_comment ].first = comment.author;
        _comments[ _last_comment ].second = comment.permlink;
        _last_comment = ( _last_comment + 1 ) % COLONY_COMMENT_BUFFER;
      }
    }
  }
}

} // detail

colony_plugin::colony_plugin() {}

colony_plugin::~colony_plugin() {}

void colony_plugin::set_program_options(
  boost::program_options::options_description& cli,
  boost::program_options::options_description& cfg
  )
{
  cfg.add_options()
    ( "colony-sign-with", bpo::value<vector<std::string>>()->composing()->multitoken(), "WIF PRIVATE KEY to be used to sign each transaction." )
    ( "colony-threads", bpo::value<std::uint32_t>()->default_value( COLONY_DEFAULT_THREADS ), ( "Number of worker threads. Default is " + std::to_string( COLONY_DEFAULT_THREADS ) ).c_str() )
    ( "colony-transactions-per-block", bpo::value<std::uint32_t>(), "Max number of transactions produced per block. When not set it will be sum of weights of individual types." )
    ( "colony-start-at-block", bpo::value<std::uint32_t>()->default_value( 0 ), "Start producing transactions when block with given number becomes head block (or right at the start if the block already passed)." )
    ( "colony-no-broadcast", bpo::value<bool>()->default_value( false ), "Disables broadcasting of produced transactions - only local witness will include them in block." )
    ( "colony-article", bpo::value<std::string>(), "Size and frequency parameters of article transactions." )
    ( "colony-reply", bpo::value<std::string>(), "Size and frequency parameters of reply transactions." )
    ( "colony-vote", bpo::value<std::string>(), "Size and frequency parameters of vote transactions." )
    ( "colony-transfer", bpo::value<std::string>(), "Size and frequency parameters of transfer transactions." )
    ( "colony-custom", bpo::value<std::string>(), "Size and frequency parameters of custom_json transactions. "
      "If no other transaction type is requested, minimal custom jsons will be produced." )
    ;
}

void colony_plugin::plugin_initialize( const boost::program_options::variables_map& options )
{
  try
  {
    ilog( "Initializing colony plugin" );
#ifndef USE_ALTERNATE_CHAIN_ID
    FC_ASSERT( false, "Are you sure you want to use colony plugin on mainnet?" );
#endif

    my = std::make_unique< detail::colony_plugin_impl >( *this, get_app() );

    if( options.count( "colony-sign-with" ) )
    {
      const std::vector<std::string> keys = options[ "colony-sign-with" ].as<std::vector<std::string>>();
      for( const std::string& wif_key : keys )
      {
        fc::optional<fc::ecc::private_key> private_key = fc::ecc::private_key::wif_to_key( wif_key );
        FC_ASSERT( private_key.valid(), "unable to parse private key" );
        my->_sign_with.emplace_back( *private_key );
      }
    }

    {
      uint32_t threads = options.at( "colony-threads" ).as<std::uint32_t>();
      FC_ASSERT( threads > 0 && threads <= 128, "At least one worker thread is needed, but no more than 128" );
      my->_max_threads = threads;
    }

    uint64_t total_weight = 0;
    if( options.count( "colony-article" ) )
    {
      fc::variant var = fc::json::from_string( options.at( "colony-article" ).as<std::string>(), fc::json::format_validation_mode::full, fc::json::strict_parser );
      my->_params[ ARTICLE ] = var.as< operation_params >();
      my->_params[ ARTICLE ].validate( 60000 );
      total_weight += my->_params[ ARTICLE ].weight;
    }
    if( options.count( "colony-reply" ) )
    {
      fc::variant var = fc::json::from_string( options.at( "colony-reply" ).as<std::string>(), fc::json::format_validation_mode::full, fc::json::strict_parser );
      my->_params[ REPLY ] = var.as< operation_params >();
      my->_params[ REPLY ].validate( 20000 );
      total_weight += my->_params[ REPLY ].weight;
    }
    if( options.count( "colony-vote" ) )
    {
      fc::variant var = fc::json::from_string( options.at( "colony-vote" ).as<std::string>(), fc::json::format_validation_mode::full, fc::json::strict_parser );
      my->_params[ VOTE ] = var.as< operation_params >();
      my->_params[ VOTE ].validate( 0 );
      total_weight += my->_params[ VOTE ].weight;
    }
    if( options.count( "colony-transfer" ) )
    {
      fc::variant var = fc::json::from_string( options.at( "colony-transfer" ).as<std::string>(), fc::json::format_validation_mode::full, fc::json::strict_parser );
      my->_params[ TRANSFER ] = var.as< operation_params >();
      my->_params[ TRANSFER ].validate( HIVE_MAX_MEMO_SIZE - 1 );
      total_weight += my->_params[ TRANSFER ].weight;
    }
    if( options.count( "colony-custom" ) )
    {
      fc::variant var = fc::json::from_string( options.at( "colony-custom" ).as<std::string>(), fc::json::format_validation_mode::full, fc::json::strict_parser );
      my->_params[ CUSTOM_JSON ] = var.as< operation_params >();
      my->_params[ CUSTOM_JSON ].validate( HIVE_CUSTOM_OP_DATA_MAX_LENGTH - sizeof( "{'v':''}" ) );
      total_weight += my->_params[ CUSTOM_JSON ].weight;
    }
    if( total_weight == 0 )
    {
      ilog( "No type of operation was requested explicitly to be produced by colony workers - using default setting for custom_jsons." );
      my->_params[ CUSTOM_JSON ] = { 0, 0, COLONY_DEFAULT_MAX_TRANSACTIONS, 1.0 };
      total_weight += my->_params[ CUSTOM_JSON ].weight;
    }

    FC_ASSERT( total_weight <= RAND_MAX, "Sum of weights should be smaller than ${x}, current value: ${t}",
      ( "x", RAND_MAX )( "t", total_weight ) );
    my->_total_weight = total_weight;

    if( options.count( "colony-transactions-per-block" ) )
      my->_max_tx_per_block = my->_dynamic_tx_per_block = options.at( "colony-transactions-per-block" ).as<std::uint32_t>();
    else
      my->_max_tx_per_block = my->_dynamic_tx_per_block = my->_total_weight;

    my->_start_at_block = options.at( "colony-start-at-block" ).as<std::uint32_t>();
    my->_disable_broadcast = options.at( "colony-no-broadcast" ).as<bool>();

    // only attach to end of sync for now, so colony does not start producing during replay/sync
    // even if _start_at_block is not set/set too low
    my->_end_of_sync_conn = my->_db.add_end_of_syncing_handler( [&]()
      { my->end_of_sync(); }, *this, 0 );
  }
  FC_CAPTURE_AND_RETHROW()
}

void colony_plugin::plugin_startup()
{
  ilog( "Colony plugin waiting for end of sync signal to start" );
}

void colony_plugin::plugin_shutdown()
{
  for( const auto& thread : my->_threads )
    thread.print_stats();

  chain::util::disconnect_signal( my->_end_of_sync_conn );
  chain::util::disconnect_signal( my->_post_apply_transaction_conn );
  chain::util::disconnect_signal( my->_post_apply_block_conn );
}

} } } // hive::plugins::colony

FC_REFLECT( hive::plugins::colony::operation_params, (min)(max)(weight)(exponent) )
