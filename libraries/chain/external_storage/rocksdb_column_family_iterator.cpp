#include <hive/chain/external_storage/rocksdb_column_family_iterator.hpp>
#include <hive/chain/external_storage/account_rocksdb_objects.hpp>
#include <hive/chain/external_storage/types.hpp>

//#define DBG_INFO
namespace hive { namespace chain {

rocksdb_column_family_iterator::rocksdb_column_family_iterator( const chainbase::database& db, ColumnTypes column_type,
              rocksdb_account_column_family_iterator_provider::ptr _provider, external_storage_reader_writer::ptr reader )
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

std::shared_ptr<account_object> rocksdb_column_family_iterator::get_account( const std::string& message, const account_name_type& account_name )
{
#ifdef DBG_INFO
  ilog("account name from `${message}`: ${acc}",(message)("acc", _obj.name));
#endif

  PinnableSlice _buffer;
  bool _read = reader->read( ColumnTypes::ACCOUNT, account_name_slice_t( account_name.data ), _buffer );

#ifdef DBG_INFO
  ilog("account name `${message}`: ${_read}", (message)("_read", _read));
#endif

  FC_ASSERT( _read );

  rocksdb_account_object _main_obj;

  load( _main_obj, _buffer.data(), _buffer.size() );
  return _main_obj.build( db );
}

rocksdb_column_family_iterator_by_next_vesting_withdrawal::rocksdb_column_family_iterator_by_next_vesting_withdrawal( const chainbase::database& db, ColumnTypes column_type,
                rocksdb_account_column_family_iterator_provider::ptr provider, external_storage_reader_writer::ptr reader )
: rocksdb_column_family_iterator( db, column_type, provider, reader )
{
}

std::shared_ptr<account_object> rocksdb_column_family_iterator_by_next_vesting_withdrawal::get()
{
  rocksdb_account_object_by_next_vesting_withdrawal _obj;

  load( _obj, it->value().data(), it->value().size() );

  static const std::string _message = "rocksdb_account_object_by_next_vesting_withdrawal";
  return rocksdb_column_family_iterator::get_account( _message, _obj.name );
}

rocksdb_column_family_iterator_by_delayed_voting::rocksdb_column_family_iterator_by_delayed_voting( const chainbase::database& db, ColumnTypes column_type,
                rocksdb_account_column_family_iterator_provider::ptr provider, external_storage_reader_writer::ptr reader )
: rocksdb_column_family_iterator( db, column_type, provider, reader )
{
}

std::shared_ptr<account_object> rocksdb_column_family_iterator_by_delayed_voting::get()
{
  rocksdb_account_object_by_delayed_voting _obj;

  load( _obj, it->value().data(), it->value().size() );

  static const std::string _message = "rocksdb_account_object_by_delayed_voting";
  return rocksdb_column_family_iterator::get_account( _message, _obj.name );
}

rocksdb_column_family_iterator_by_governance_vote_expiration_ts::rocksdb_column_family_iterator_by_governance_vote_expiration_ts( const chainbase::database& db, ColumnTypes column_type,
                rocksdb_account_column_family_iterator_provider::ptr provider, external_storage_reader_writer::ptr reader )
: rocksdb_column_family_iterator( db, column_type, provider, reader )
{
}

std::shared_ptr<account_object> rocksdb_column_family_iterator_by_governance_vote_expiration_ts::get()
{
  rocksdb_account_object_by_governance_vote_expiration_ts _obj;

  load( _obj, it->value().data(), it->value().size() );

  static const std::string _message = "rocksdb_account_object_by_governance_vote_expiration_ts";
  return rocksdb_column_family_iterator::get_account( _message, _obj.name );
}

}}
