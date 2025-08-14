#pragma once

#include <hive/chain/external_storage/account_item.hpp>
#include <hive/chain/external_storage/account_rocksdb_objects.hpp>
#include <hive/chain/external_storage/rocksdb_account_iterator.hpp>
#include <hive/chain/external_storage/rocksdb_iterator_provider.hpp>
namespace hive { namespace chain {

template<typename ByIndex>
class account_iterator
{
  private:

    enum { shm_storage, volatile_storage, rocksdb_storage } last;

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

    bool cmp( const account_object& a, const account_object& b );
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
                                              : last( shm_storage ),
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
bool account_iterator<ByIndex>::cmp( const account_object& a, const account_object& b )
{
  return a.get_next_vesting_withdrawal() < b.get_next_vesting_withdrawal();
}

template<typename ByIndex>
void account_iterator<ByIndex>::execute_cmp()
{
  if( end() )
    return;

  if( it == index.end() || volatile_it == volatile_index.end() || rocksdb_iterator->end() )
  {
    if( it == index.end() )
    {
      if( volatile_it == volatile_index.end() || rocksdb_iterator->end() )
        last = volatile_it == volatile_index.end() ? rocksdb_storage : volatile_storage;
      else
        last = cmp( *volatile_item, *rocksdb_item ) ? volatile_storage : rocksdb_storage;
    }
    else if( volatile_it == volatile_index.end() )
    {
      if( it == index.end() || rocksdb_iterator->end() )
        last = it == index.end() ? rocksdb_storage : shm_storage;
      else
        last = cmp( *it, *rocksdb_item ) ? shm_storage : rocksdb_storage;
    }
    else
    {
      if( it == index.end() || volatile_it == volatile_index.end() )
        last = it == index.end() ? volatile_storage : shm_storage;
      else
        last = cmp( *it, *volatile_item ) ? shm_storage : volatile_storage;
    }
  }
  else
  {
    last = cmp( *it, *volatile_item ) ? shm_storage : volatile_storage;
    if( last == shm_storage )
      last = cmp( *it, *rocksdb_item ) ? shm_storage : rocksdb_storage;
    else
      last = cmp( *it, *volatile_item ) ? volatile_storage : rocksdb_storage;
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
  if( last == shm_storage )
  {
    return account( &(*it) );
  }
  else if( last == volatile_storage )
  {
    return account( volatile_item );
  }
  else
  {
    return account( rocksdb_item );
  }
}

template<typename ByIndex>
void account_iterator<ByIndex>::next()
{
  if( last == shm_storage )
  {
    ++it;
  }
  else if( last == volatile_storage )
  {
    ++volatile_it;
    update_volatile_item();
  }
  else
  {
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
