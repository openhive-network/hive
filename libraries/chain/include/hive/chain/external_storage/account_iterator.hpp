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

    enum { shm_storage, volatile_storage } last;

    const chainbase::database& db;

    using index_t = decltype( std::declval<chainbase::generic_index<account_index>>().indicies().template get<ByIndex>() );
    using volatile_index_t = decltype( std::declval<chainbase::generic_index<volatile_account_index>>().indicies().template get<ByIndex>() );

    using iterator_t = decltype( std::declval<index_t>().begin() );
    using volatile_iterator_t = decltype( std::declval<volatile_index_t>().begin() );

    rocksdb_account_iterator::ptr rocksdb_iterator;

    const index_t& index;
    const volatile_index_t& volatile_index;

    iterator_t it;
    volatile_iterator_t volatile_it;

    template<typename Object_Type1, typename Object_Type2>
    bool cmp( const Object_Type1& a, const Object_Type2& b );
    void execute_cmp();

  public:

    account_iterator( const chainbase::database& db, rocksdb_account_column_family_iterator_provider::ptr provider );

    account begin();
    account get();
    void next();
    bool end();
};

template<typename ByIndex>
account_iterator<ByIndex>::account_iterator( const chainbase::database& db, rocksdb_account_column_family_iterator_provider::ptr provider )
                                                : last( shm_storage ),
                                                  db( db ),
                                                  rocksdb_iterator( rocksdb_iterator_provider<ByIndex>::get_iterator( provider ) ),
                                                  index( db.get_index< account_index, ByIndex >() ),
                                                  volatile_index( db.get_index< volatile_account_index, ByIndex >() )
{
}

template<typename ByIndex>
template<typename Object_Type1, typename Object_Type2>
bool account_iterator<ByIndex>::cmp( const Object_Type1& a, const Object_Type2& b )
{
  return a.get_next_vesting_withdrawal() < b.get_next_vesting_withdrawal();
}

template<typename ByIndex>
void account_iterator<ByIndex>::execute_cmp()
{
  if( end() )
    return;

  if( it == index.end() || volatile_it == volatile_index.end() )
  {
    last = it == index.end() ? volatile_storage : shm_storage;
    return;
  }

  last = cmp( *it, *volatile_it ) ? shm_storage : volatile_storage;
}

template<typename ByIndex>
account account_iterator<ByIndex>::begin()
{
  it = index.begin();
  volatile_it = volatile_index.begin();

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
  else
  {
    return account( volatile_it->read() );
  }
}

template<typename ByIndex>
void account_iterator<ByIndex>::next()
{
  if( last == shm_storage )
  {
    ++it;
  }
  else
  {
    ++volatile_it;
  }
  execute_cmp();
}

template<typename ByIndex>
bool account_iterator<ByIndex>::end()
{
  return it == index.end() && volatile_it == volatile_index.end();
}

template<typename ByIndex>
std::shared_ptr<account_iterator<ByIndex>> get_iterator()
{
  return std::shared_ptr<account_iterator<ByIndex>>();
}

}}
