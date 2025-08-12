#include <hive/chain/external_storage/rocksdb_column_family_iterator.hpp>

namespace hive { namespace chain {

rocksdb_column_family_iterator::rocksdb_column_family_iterator( ColumnTypes _column_type, rocksdb_account_column_family_iterator_provider::ptr& _provider )
: column_type( _column_type )
{
  it = _provider->create_column_family_iterator( _column_type );
}

account rocksdb_column_family_iterator::begin()
{
  it->SeekToFirst();
  return get();
}

void rocksdb_column_family_iterator::next()
{
  it->Next();
}

bool rocksdb_column_family_iterator::end()
{
  return it->Valid();
}

rocksdb_column_family_iterator_by_next_vesting_withdrawal::rocksdb_column_family_iterator_by_next_vesting_withdrawal( ColumnTypes _column_type, rocksdb_account_column_family_iterator_provider::ptr& _provider )
: rocksdb_column_family_iterator( _column_type, _provider ) 
{
}

account rocksdb_column_family_iterator_by_next_vesting_withdrawal::get()
{
//auto _x = rocksdb_reader<ColumnTypes::ACCOUNT_BY_NEXT_VESTING_WITHDRAWAL>::read( it->value() );
  return account();
}

}}
