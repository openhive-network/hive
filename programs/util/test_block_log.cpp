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

    std::vector<std::shared_ptr<hive::chain::full_transaction_type>> full_txs;

    hive::protocol::signed_block_header b1;
    b1.witness = "alice";
    b1.previous = hive::protocol::block_id_type();

    std::shared_ptr<hive::chain::full_block_type> fb1 =
      hive::chain::full_block_type::create_from_block_header_and_transactions(b1, full_txs, nullptr);

    log.append(fb1);
    log.flush();
    idump((fb1->get_block()));
    dump_head(log);
    idump((fb1->get_uncompressed_block_size()));

    hive::protocol::signed_block_header b2;
    b2.witness = "bob";
    b2.previous = fb1->get_block_id();

    std::shared_ptr<hive::chain::full_block_type> fb2 =
      hive::chain::full_block_type::create_from_block_header_and_transactions(b2, full_txs, nullptr);

    log.append(fb2);
    log.flush();
    idump((fb2->get_block()));
    dump_head(log);
    idump((fb2->get_uncompressed_block_size()));

    std::shared_ptr<hive::chain::full_block_type> r1 = log.read_block_by_num(1);
    idump((r1->get_block()));
    idump((r1->get_uncompressed_block_size()));

    std::shared_ptr<hive::chain::full_block_type> r2 = log.read_block_by_num(2);
    idump((r2->get_block()));
    idump((r2->get_uncompressed_block_size()));

    idump((log.read_head()->get_block()));
    idump((log.read_head()->get_uncompressed_block_size()));

    std::shared_ptr<hive::chain::full_block_type> r3 = log.read_block_by_num(3);
    idump(((bool)r3));
  }
  catch ( const std::exception& e )
  {
    edump( ( std::string( e.what() ) ) );
  }

  return 0;
}
