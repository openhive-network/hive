#pragma once

#include <hive/chain/external_storage/account_item.hpp>
#include <hive/chain/external_storage/account_rocksdb_objects.hpp>
#include <hive/chain/external_storage/rocksdb_account_iterator.hpp>
#include <hive/chain/external_storage/rocksdb_iterator_provider.hpp>
namespace hive { namespace chain {

//#define LOG_DEBUG

#ifdef LOG_DEBUG

#define LOG0(message) \
{ \
  ilog( "${m}",("m", message)); \
}

#define LOG1(message, a) \
{ \
  ilog( "${m} [${name} ${time}]",("m", message)("name", a.get_name())("time", a.get_next_vesting_withdrawal())); \
}

#define LOG2(message, val, a, b) \
{ \
  ilog( "${m} ${val} [${name} ${time}] [${name2} ${time2}]", \
  ("m", message) \
  ("name", a.get_name())("time", a.get_next_vesting_withdrawal()) \
  ("name2", b.get_name())("time2", b.get_next_vesting_withdrawal()) \
  ("val", val) \
  ); \
}

#define LOG3(message, a, b, val) \
{ \
  ilog( "${m} ${val} [${name} ${time}] [${name2} ${time2}]", \
  ("m", message) \
  ("name", a.get_name())("time", a.get_next_vesting_withdrawal()) \
  ("name2", b.get_name())("time2", b.get_next_vesting_withdrawal()) \
  ("val", val) \
  ); \
}

#define LOG4(message, val) \
{ \
    ilog( "${m} ${val}", \
    ("m", message) \
    ("val", val) \
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
class helper
{
};

template<>
class helper<by_next_vesting_withdrawal>
{
  public:

    static bool cmp( const account_object& a, const account_object& b )
    {
      if( a.get_next_vesting_withdrawal() == b.get_next_vesting_withdrawal())
        return a.get_name() < b.get_name();
      else
        return a.get_next_vesting_withdrawal() < b.get_next_vesting_withdrawal();
    }

    static bool equal( const account_object& a, const account_object& b )
    {
      return a.get_next_vesting_withdrawal() == b.get_next_vesting_withdrawal() && a.get_name() == b.get_name();
    }
};

template<typename ByIndex>
class account_iterator
{
  private:

    enum { none, shm_storage, volatile_storage, rocksdb_storage } last;

    const chainbase::database& db;

    using index_t = decltype( std::declval<chainbase::generic_index<account_index>>().indicies().template get<ByIndex>() );
    using volatile_index_t = decltype( std::declval<chainbase::generic_index<volatile_account_index>>().indicies().template get<ByIndex>() );

    using iterator_t = decltype( std::declval<index_t>().begin() );
    using volatile_iterator_t = decltype( std::declval<volatile_index_t>().begin() );

    const index_t& index;
    const volatile_index_t& volatile_index;

    iterator_t it;
    volatile_iterator_t volatile_it;
    rocksdb_account_iterator::ptr rocksdb_iterator;

    std::shared_ptr<account_object> rocksdb_item;
    std::shared_ptr<account_object> volatile_item;

    void execute_cmp();

    void update_volatile_item();
    void update_rocksdb_item();

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
                                                index( db.get_index< account_index, ByIndex >() ),
                                                volatile_index( db.get_index< volatile_account_index, ByIndex >() ),
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
void account_iterator<ByIndex>::execute_cmp()
{
  if( end() )
  {
    last = none;
    return;
  }

  if( it == index.end() || volatile_it == volatile_index.end() || rocksdb_iterator->end() )
  {
    if( it == index.end() )
    {
      if( volatile_it == volatile_index.end() || rocksdb_iterator->end() )
      {
        last = volatile_it == volatile_index.end() ? rocksdb_storage : volatile_storage;
        LOG4( "cmp-a", last )
      }
      else
      {
        last = helper<ByIndex>::cmp( *volatile_item, *rocksdb_item ) ? volatile_storage : rocksdb_storage;
        LOG3( "cmp-b", (*volatile_item), (*rocksdb_item), last )
      }
    }
    else if( volatile_it == volatile_index.end() )
    {
      if( it == index.end() || rocksdb_iterator->end() )
      {
        last = it == index.end() ? rocksdb_storage : shm_storage;
        LOG4( "cmp-c", last )
      }
      else
      {
        last = helper<ByIndex>::cmp( *it, *rocksdb_item ) ? shm_storage : rocksdb_storage;
        LOG3( "cmp-d", (*it), (*rocksdb_item), last )
      }
    }
    else
    {
      if( it == index.end() || volatile_it == volatile_index.end() )
      {
        last = it == index.end() ? volatile_storage : shm_storage;
        LOG4( "cmp-e", last )
      }
      else
      {
        last = helper<ByIndex>::cmp( *it, *volatile_item ) ? shm_storage : volatile_storage;
        LOG3( "cmp-f", (*it), (*volatile_item), last )
      }
    }
  }
  else
  {
    last = helper<ByIndex>::cmp( *it, *volatile_item ) ? shm_storage : volatile_storage;
    LOG3( "cmp-g", (*it), (*volatile_item), last )
    if( last == shm_storage )
    {
      last = helper<ByIndex>::cmp( *it, *rocksdb_item ) ? shm_storage : rocksdb_storage;
      LOG3( "cmp-h", (*it), (*rocksdb_item), last )
    }
    else
    {
      last = helper<ByIndex>::cmp( *volatile_item, *rocksdb_item ) ? volatile_storage : rocksdb_storage;
      LOG3( "cmp-i", (*it), (*rocksdb_item), last )
    }
  }

}

template<typename ByIndex>
account account_iterator<ByIndex>::begin()
{
  it = index.begin();
  volatile_it = volatile_index.begin();
  rocksdb_iterator->begin();

  update_volatile_item();
  update_rocksdb_item();

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

  if( last == shm_storage )
  {
    LOG1("SHM:", (*it))
    return account( &(*it) );
  }
  else if( last == volatile_storage )
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

  auto _move_shm_iterator = [this]( const account_object& obj )
  {
    LOG2("MOVE SHM", helper<ByIndex>::equal( obj, *it ), obj, (*it) )
    if( helper<ByIndex>::equal( obj, *it ) )
      ++it;
  };

  auto _move_volatile_iterator = [this]( const account_object& obj )
  {
    LOG2("MOVE VOLATILE", helper<ByIndex>::equal( obj, *volatile_item ), obj, (*volatile_item) )
    if( helper<ByIndex>::equal( obj, *volatile_item ) )
    {
      ++volatile_it;
      update_volatile_item();
    }
  };

  auto _move_rocksdb_iterator = [this]( const account_object& obj )
  {
    LOG2("MOVE RB", helper<ByIndex>::equal( obj, *rocksdb_item ), obj, (*rocksdb_item) )
    if( helper<ByIndex>::equal( obj, *rocksdb_item ) )
    {
      rocksdb_iterator->next();
      update_rocksdb_item();
    }
  };

  if( last == shm_storage )
  {
    LOG0("NEXT SHM")
    _move_volatile_iterator( *it );
    _move_rocksdb_iterator( *it );

    ++it;
  }
  else if( last == volatile_storage )
  {
    LOG0("NEXT VOLATILE")
    _move_shm_iterator( *volatile_item );
    _move_rocksdb_iterator( *volatile_item );

    ++volatile_it;
    update_volatile_item();
  }
  else
  {
    LOG0("NEXT RB")
    _move_shm_iterator( *rocksdb_item );
    _move_volatile_iterator( *rocksdb_item );

    rocksdb_iterator->next();
    update_rocksdb_item();
  }

  execute_cmp();
}

template<typename ByIndex>
bool account_iterator<ByIndex>::end()
{
  return it == index.end() && volatile_it == volatile_index.end() && rocksdb_iterator->end();
}

}}
