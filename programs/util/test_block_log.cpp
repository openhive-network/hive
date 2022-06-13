#include <hive/chain/database.hpp>
#include <hive/chain/full_block.hpp>
#include <hive/protocol/block.hpp>
#include <fc/io/raw.hpp>

void dump_head(const hive::chain::block_log& log)
{
  std::shared_ptr<hive::chain::full_block_type> head = log.head();
  if (head)
    ilog("head: ${head}", ("head", head->get_block()));
  else
    ilog("head: null");
}

int main( int argc, char** argv, char** envp )
{
  try
  {
    //hive::chain::database db;
    hive::chain::block_log log;

    fc::temp_directory temp_dir( "." );

    //db.open( temp_dir );
    log.open( temp_dir.path() / "log" );
 
    dump_head(log);

    hive::protocol::signed_block b1;
    b1.witness = "alice";
    b1.previous = hive::protocol::block_id_type();

    std::shared_ptr<hive::chain::full_block_type> fb1 = hive::chain::full_block_type::create_from_signed_block(b1);

    log.append(fb1);
    log.flush();
    idump((b1));
    dump_head(log);
    idump((fc::raw::pack_size(b1)));

    hive::protocol::signed_block b2;
    b2.witness = "bob";
    b2.previous = b1.legacy_id();

    std::shared_ptr<hive::chain::full_block_type> fb2 = hive::chain::full_block_type::create_from_signed_block(b2);

    log.append(fb2);
    log.flush();
    idump((b2));
    dump_head(log);
    idump((fc::raw::pack_size(b2)));

    std::shared_ptr<hive::chain::full_block_type> r1 = log.read_block_by_num(1);
    idump((r1->get_block()));
    idump((fc::raw::pack_size(r1->get_block())));

    std::shared_ptr<hive::chain::full_block_type> r2 = log.read_block_by_num(2);
    idump( (r2->get_block()) );
    idump( (fc::raw::pack_size(r2->get_block())) );

    idump((log.read_head()->get_block()));
    idump((fc::raw::pack_size(log.read_head()->get_block())));

    std::shared_ptr<hive::chain::full_block_type> r3 = log.read_block_by_num(3);
    idump(((bool)r3));
  }
  catch ( const std::exception& e )
  {
    edump( ( std::string( e.what() ) ) );
  }

  return 0;
}
