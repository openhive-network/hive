#if defined IS_TEST_NET && !defined ENABLE_MIRA && !defined ENABLE_STD_ALLOCATOR
#include <boost/test/unit_test.hpp>

#include "../db_fixture/database_fixture.hpp"

#include <hive/chain/comment_object.hpp>

#include <future>
#include <thread>

using namespace hive::protocol;
using namespace hive::chain;

BOOST_FIXTURE_TEST_SUITE( db_lock_tests, clean_database_fixture )

BOOST_AUTO_TEST_CASE( push_transactions_and_read_api ) {
  using namespace std::chrono_literals;

  std::atomic_bool stop_read_loops( false );
  std::atomic_int comment_loop_counter( 0 );

  try {
    BOOST_TEST_MESSAGE( "Database lock test - add comments and votes in one thread and read it within two other threads" );
    constexpr auto NUMBER_OF_DB_MODIFICATIONS_ITERATIONS = 1'000;

    auto add_comment_loop = [&](){
      for ( auto  i =0; i < NUMBER_OF_DB_MODIFICATIONS_ITERATIONS; i++) {
        comment_loop_counter.store( i );
        auto guard_code = [&](){
          // block generation is required for big number of iterations
          if (!(i % 10))
            generate_blocks(1);

          auto author = std::string("alice") + std::to_string(i);
          auto voter = std::string("bob") + std::to_string(i);
          auto author_permlink = std::to_string(i) + "alicepermlink";

          fc::ecc::private_key author_private_key = generate_private_key(author);
          fc::ecc::private_key author_post_key = generate_private_key(author + "_post");
          public_key_type author_public_key = author_private_key.get_public_key();
          account_create(author, author_public_key, author_post_key.get_public_key());

          fc::ecc::private_key voter_private_key = generate_private_key(voter);
          fc::ecc::private_key voter_post_key = generate_private_key(voter + "_post");
          account_create(voter, author_public_key, voter_post_key.get_public_key());

          // author create a new comment
          {
            comment_operation op;
            op.author = author;
            op.permlink = author_permlink;
            op.parent_author = "";
            op.parent_permlink = "ipsum";
            op.title = "Lorem Ipsum";
            op.body = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
            op.json_metadata = "{\"foo\":\"bar\"}";
            signed_transaction tx;
            tx.operations.push_back(op);
            tx.set_expiration(db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION);
            sign(tx, author_private_key);
            db->push_transaction(tx, database::skip_transaction_dupe_check | database::skip_validate |
                                     database::skip_transaction_signatures);
          }

          {
            vote_operation op;
            op.voter = voter;
            op.author = author;
            op.permlink = author_permlink;
            op.weight = HIVE_100_PERCENT;

            signed_transaction tx;
            tx.set_expiration(db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION);
            tx.operations.push_back(op);
            sign(tx, voter_private_key);
            db->push_transaction(tx, database::skip_transaction_dupe_check | database::skip_validate |
                                     database::skip_authority_check);
          }
        };

        db->with_write_lock(std::move(guard_code));
      }
    };

    auto iterate_comments = [&](){
      std::ostringstream ss;
      ss << std::this_thread::get_id();
      do
        {
        const auto& comment_idx = db->get_index< comment_index >().indices().get< by_id >();

          auto guarded_loop = [&]()
          {
              // iterates through all comments
              auto expected_comment_id(0u);
              for (auto it = comment_idx.begin(); it != comment_idx.end(); ++it)
              {
                if ( it->get_id() != expected_comment_id ) {
                  throw std::runtime_error(
                          std::string( "In read comment thread " ) + ss.str() +
                          " on comment add iteration: " + std::to_string( comment_loop_counter.load() ) +
                          " comment id did not match expected value: " +
                          std::to_string( it->get_id() ) + "!=" + std::to_string( expected_comment_id )
                  );
                }
                ++expected_comment_id;
              }

          };

          try
          {
            db->with_read_lock( std::move( guarded_loop ) );
          }
          catch ( chainbase::lock_exception& )
          {
            /* lock_exception is ignored. It is only thrown when during 1s lock for db reading cannot be aquired.
             * It is only thrown for information purpose (for example to inform an API).
             * The situation doesn't affect the process memory and doesn't cause the program failure.
             */
          }

      } while ( !stop_read_loops.load()  );
    };

    // start comments adding thread
    auto comment_add_loop_future = std::async(std::launch::async, add_comment_loop );

    // start few reading threads
    auto comment_read_loop_future1 = std::async( std::launch::async, iterate_comments );
    auto comment_read_loop_future2 = std::async( std::launch::async, iterate_comments );
    auto comment_read_loop_future3 = std::async( std::launch::async, iterate_comments );
    auto comment_read_loop_future4 = std::async( std::launch::async, iterate_comments );
    auto comment_read_loop_future5 = std::async( std::launch::async, iterate_comments );

    comment_add_loop_future.get();

    // to stop reading threads
    stop_read_loops.store( true );

    comment_read_loop_future1.get();
    comment_read_loop_future2.get();
    comment_read_loop_future3.get();
    comment_read_loop_future4.get();
    comment_read_loop_future5.get();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
