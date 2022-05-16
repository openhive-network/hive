#include <hive/chain/database.hpp>
#include <hive/protocol/block.hpp>
#include <fc/io/raw.hpp>

void dump_head(const hive::chain::block_log& log)
{
  boost::shared_ptr<hive::chain::signed_block> head = log.head();
  if (head)
    ilog("head: ${head}", ("head", *head));
  else
    ilog("head: null");
}

int main( int argc, char** argv, char** envp )
{
  try
  {
    //TODO : These flags should be set by a command line.
    fc::raw::pack_flags _flags;

    //hive::chain::database db;
    hive::chain::block_log log;

    fc::temp_directory temp_dir( "." );

    //db.open( temp_dir );
    log.open( temp_dir.path() / "log" );
 
    dump_head(log);

    hive::protocol::signed_block b1;
    b1.witness = "alice";
    b1.previous = hive::protocol::block_id_type();

    log.append( b1, _flags );
    log.flush();
    idump( (b1) );
    dump_head(log);
    idump( (fc::raw::pack_size(b1, _flags)) );

    hive::protocol::signed_block b2;
    b2.witness = "bob";
    b2.previous = b1.id( _flags );

    log.append( b2, _flags );
    log.flush();
    idump( (b2) );
    dump_head(log);
    idump( (fc::raw::pack_size(b2, _flags)) );

    auto r1 = log.read_block_by_num( 1 );
    idump( (r1) );
    idump( (fc::raw::pack_size(*r1, _flags)) );

    auto r2 = log.read_block_by_num( 2 );
    idump( (r2) );
    idump( (fc::raw::pack_size(*r2, _flags)) );

    idump( (log.read_head()) );
    idump( (fc::raw::pack_size(log.read_head(), _flags)));

    auto r3 = log.read_block_by_num( 3 );
    idump( (r3) );
  }
  catch ( const std::exception& e )
  {
    edump( ( std::string( e.what() ) ) );
  }

  return 0;
}
