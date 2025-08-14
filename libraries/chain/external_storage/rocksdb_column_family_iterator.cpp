#include <hive/chain/external_storage/rocksdb_column_family_iterator.hpp>
#include <hive/chain/external_storage/account_rocksdb_objects.hpp>
#include <hive/chain/external_storage/types.hpp>

namespace hive { namespace chain {

rocksdb_column_family_iterator::rocksdb_column_family_iterator( const chainbase::database& db, ColumnTypes column_type,
              rocksdb_account_column_family_iterator_provider::ptr& _provider, external_storage_reader_writer::ptr& reader )
              : db( db ), reader( reader )
{
  it = _provider->create_column_family_iterator( column_type );
}

void rocksdb_column_family_iterator::begin()
{
  it->SeekToFirst();
}

void rocksdb_column_family_iterator::next()
{
  if( !end() )
    it->Next();
}

bool rocksdb_column_family_iterator::end()
{
  return !it->Valid();
}

rocksdb_column_family_iterator_by_next_vesting_withdrawal::rocksdb_column_family_iterator_by_next_vesting_withdrawal( const chainbase::database& db, ColumnTypes column_type,
                rocksdb_account_column_family_iterator_provider::ptr& provider, external_storage_reader_writer::ptr& reader )
: rocksdb_column_family_iterator( db, column_type, provider, reader )
{
}

std::shared_ptr<account_object> rocksdb_column_family_iterator_by_next_vesting_withdrawal::get()
{
  rocksdb_account_object_by_next_vesting_withdrawal _obj;

  load( _obj, it->value().data(), it->value().size() );

  PinnableSlice _buffer;
  FC_ASSERT( reader->read( ColumnTypes::ACCOUNT, account_name_slice_t( _obj.name.data ), _buffer ) );

  rocksdb_account_object _main_obj;

  load( _main_obj, _buffer.data(), _buffer.size() );
  return _main_obj.build( db );
}

}}
