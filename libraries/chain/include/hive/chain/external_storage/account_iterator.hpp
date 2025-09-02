#pragma once

#include <hive/chain/external_storage/account_item.hpp>
#include <hive/chain/external_storage/account_rocksdb_objects.hpp>
#include <hive/chain/external_storage/rocksdb_account_iterator.hpp>
#include <hive/chain/external_storage/rocksdb_iterator_provider.hpp>
namespace hive { namespace chain {

template<typename ByIndex>
class helper
{
};

template<>
class helper<by_next_vesting_withdrawal>
{
  public:

    static bool cmp( const account_object& a, const account_object& b )
    {
      if( a.get_next_vesting_withdrawal() == b.get_next_vesting_withdrawal() )
        return a.get_name() < b.get_name();
      else
        return a.get_next_vesting_withdrawal() < b.get_next_vesting_withdrawal();
    }

    static bool equal( const account_object& a, const account_object& b )
    {
      return a.get_next_vesting_withdrawal() == b.get_next_vesting_withdrawal() && a.get_name() == b.get_name();
    }

    static bool is_obsolete_value( const volatile_account_object& obj )
    {
      return obj.get_previous_next_vesting_withdrawal().has_value();
    }

    static time_point_sec get_time( const account_object& a )
    {
      return a.get_next_vesting_withdrawal();
    }
};

template<>
class helper<by_delayed_voting>
{
  public:

    static bool cmp( const account_object& a, const account_object& b )
    {
      if( a.get_oldest_delayed_vote_time() == b.get_oldest_delayed_vote_time() )
        return a.get_id() < b.get_id();
      else
        return a.get_oldest_delayed_vote_time() < b.get_oldest_delayed_vote_time();
    }

    static bool equal( const account_object& a, const account_object& b )
    {
      return a.get_oldest_delayed_vote_time() == b.get_oldest_delayed_vote_time() && a.get_id() == b.get_id();
    }

    static bool is_obsolete_value( const volatile_account_object& obj )
    {
      return obj.get_previous_oldest_delayed_vote_time().has_value();
    }

    static time_point_sec get_time( const account_object& a )
    {
      return a.get_oldest_delayed_vote_time();
    }
};

template<>
class helper<by_governance_vote_expiration_ts>
{
  public:

    static bool cmp( const account_object& a, const account_object& b )
    {
      if( a.get_governance_vote_expiration_ts() == b.get_governance_vote_expiration_ts() )
        return a.get_id() < b.get_id();
      else
        return a.get_governance_vote_expiration_ts() < b.get_governance_vote_expiration_ts();
    }

    static bool equal( const account_object& a, const account_object& b )
    {
      return a.get_governance_vote_expiration_ts() == b.get_governance_vote_expiration_ts() && a.get_id() == b.get_id();
    }

    static bool is_obsolete_value( const volatile_account_object& obj )
    {
      return obj.get_previous_governance_vote_expiration_ts().has_value();
    }

    static time_point_sec get_time( const account_object& a )
    {
      return a.get_governance_vote_expiration_ts();
    }
};

template<>
class helper<by_name>
{
  public:

    static bool cmp( const account_object& a, const account_object& b )
    {
      return a.get_name() < b.get_name();
    }

    static bool equal( const account_object& a, const account_object& b )
    {
      return a.get_name() == b.get_name();
    }

    static bool is_obsolete_value( const volatile_account_object& obj )
    {
      return false;
    }

    static time_point_sec get_time( const account_object& a )
    {
      return time_point_sec();
    }
};

//#define LOG_DEBUG

#ifdef LOG_DEBUG

#define LOG0(message) \
{ \
  ilog( "${m}",("m", message)); \
}

#define LOG1(message, a) \
{ \
  ilog( "${m} [${name} ${time}]",("m", message)("name", a.get_name())("time", helper<ByIndex>::get_time(a))); \
}

#define LOG2(message, val, a, b) \
{ \
  ilog( "${m} ${val} [${name} ${time}] [${name2} ${time2}]", \
  ("m", message) \
  ("name", a.get_name())("time", helper<ByIndex>::get_time(a)) \
  ("name2", b.get_name())("time2", helper<ByIndex>::get_time(b)) \
  ("val", (uint32_t)val) \
  ); \
}

#define LOG3(message, a, b, val) \
{ \
  ilog( "${m} ${val} [${name} ${time} ${balance}] [${name2} ${time2} ${balance2}]", \
  ("m", message) \
  ("name", a.get_name())("time", helper<ByIndex>::get_time(a))("balance", a.get_balance()) \
  ("name2", b.get_name())("time2", helper<ByIndex>::get_time(b))("balance2", b.get_balance()) \
  ("val", (uint32_t)val) \
  ); \
}

#define LOG4(message, val) \
{ \
    ilog( "${m} ${val}", \
    ("m", message) \
    ("val", (uint32_t)val) \
    ); \
}

#else

#define LOG0(message)
#define LOG1(message, a)
#define LOG2(message, val, a, b)
#define LOG3(message, a, b, val)
#define LOG4(message, val)

#endif

template<typename ByIndex>
class account_iterator
{
  private:

    enum { none, shm_storage, volatile_storage, rocksdb_storage } last;

    const chainbase::database& db;

    using volatile_index_t = decltype( std::declval<chainbase::generic_index<volatile_account_index>>().indicies().template get<ByIndex>() );
    using helper_index_t = decltype( std::declval<chainbase::generic_index<volatile_account_index>>().indicies().template get<by_name>() );

    using volatile_iterator_t = decltype( std::declval<volatile_index_t>().begin() );

    const volatile_index_t& volatile_index;
    const helper_index_t& helper_index;

    volatile_iterator_t volatile_it;
    rocksdb_account_iterator::ptr rocksdb_iterator;

    std::shared_ptr<account_object> rocksdb_item;
    std::shared_ptr<account_object> volatile_item;

    void execute_cmp();

    void update_volatile_item();
    void update_rocksdb_item();

    template<bool IS_BEGIN, bool UPDATE_ITERATOR>
    void move_rocksdb_iterator();

  public:

    account_iterator( const chainbase::database& db,
                      rocksdb_account_column_family_iterator_provider::ptr provider,
                      external_storage_reader_writer::ptr reader );

    account begin();
    account get();
    void next();
    bool end();
};

template<typename ByIndex>
account_iterator<ByIndex>::account_iterator(  const chainbase::database& db,
                                              rocksdb_account_column_family_iterator_provider::ptr provider,
                                              external_storage_reader_writer::ptr reader )
                                              : last( none ),
                                                db( db ),
                                                volatile_index( db.get_index< volatile_account_index, ByIndex >() ),
                                                helper_index( db.get_index< volatile_account_index, by_name >() ),
                                                rocksdb_iterator( rocksdb_iterator_provider<ByIndex>::get_iterator( db, provider, reader ) )
{
}

template<typename ByIndex>
void account_iterator<ByIndex>::update_volatile_item()
{
  if( volatile_it != volatile_index.end() )
    volatile_item = volatile_it->read();
}

template<typename ByIndex>
void account_iterator<ByIndex>::update_rocksdb_item()
{
  if( !rocksdb_iterator->end() )
    rocksdb_item = rocksdb_iterator->get();
}

template<typename ByIndex>
template<bool IS_BEGIN, bool UPDATE_ITERATOR>
void account_iterator<ByIndex>::move_rocksdb_iterator()
{
  if( UPDATE_ITERATOR )
  {
    if( IS_BEGIN )
      rocksdb_iterator->begin();
    else
      rocksdb_iterator->next();

    update_rocksdb_item();
  }

  bool _was_the_same = false;
  while( !rocksdb_iterator->end() )
  {
    if( !_was_the_same && volatile_it != volatile_index.end() && helper<ByIndex>::equal( *rocksdb_item, *volatile_item ) )
    {
      _was_the_same = true;
      LOG1("SKIP-THE-SAME", (*rocksdb_item));
      rocksdb_iterator->next();
      update_rocksdb_item();
    }
    else
    {
      auto _found = helper_index.find( rocksdb_item->get_name() );
      if( _found != helper_index.end() && helper<ByIndex>::is_obsolete_value( *_found ) )
      {
        LOG1("SKIP-OBSOLETE", (*rocksdb_item));
        rocksdb_iterator->next();
        update_rocksdb_item();
      }
      else
        break;
    }
  }
}

template<typename ByIndex>
void account_iterator<ByIndex>::execute_cmp()
{
  if( end() )
  {
    last = none;
    return;
  }

  if( volatile_it == volatile_index.end() || rocksdb_iterator->end() )
  {
    if( volatile_it == volatile_index.end() )
    {
      last = rocksdb_storage;
      LOG4( "cmp-a", last )
    }
    else
    {
      last = volatile_storage;
      LOG4( "cmp-b", last )
    }
  }
  else
  {
    last = helper<ByIndex>::cmp( *volatile_item, *rocksdb_item ) ? volatile_storage : rocksdb_storage;
    LOG3( "cmp-c", (*volatile_item), (*rocksdb_item), last )
  }

}

template<typename ByIndex>
account account_iterator<ByIndex>::begin()
{
  volatile_it = volatile_index.begin();
  update_volatile_item();

  move_rocksdb_iterator<true, true>();

  execute_cmp();

  return get();
}

template<typename ByIndex>
account account_iterator<ByIndex>::get()
{
  if( last == none )
  {
    return account();
  }

  if( last == volatile_storage )
  {
    LOG1("VOLATILE", (*volatile_item))
    return account( volatile_item );
  }
  else
  {
    LOG1("RB", (*rocksdb_item))
    return account( rocksdb_item );
  }
}

template<typename ByIndex>
void account_iterator<ByIndex>::next()
{
  FC_ASSERT( last != none, "Iterator error." );

  if( last == volatile_storage )
  {
    LOG0("NEXT VOLATILE")
    ++volatile_it;
    update_volatile_item();

    move_rocksdb_iterator<false, false>();
  }
  else
  {
    LOG0("NEXT RB")
    move_rocksdb_iterator<false, true>();
  }

  execute_cmp();
}

template<typename ByIndex>
bool account_iterator<ByIndex>::end()
{
  return volatile_it == volatile_index.end() && rocksdb_iterator->end();
}

}}
