#include <boost/test/unit_test.hpp>

#include <graphene/net/node.hpp>

using graphene::net::advertised_item_fetch_decision;
using graphene::net::advertised_item_status;
using graphene::net::decide_advertised_item_fetch;

namespace {

advertised_item_status block_advertisement()
{
  advertised_item_status status;
  status.is_block = true;
  status.new_from_this_peer = true;
  return status;
}

void check_decision( const advertised_item_fetch_decision& decision, bool queue, bool refresh_timestamp, bool wake_fetch_loop )
{
  BOOST_CHECK_EQUAL( decision.queue, queue );
  BOOST_CHECK_EQUAL( decision.refresh_timestamp, refresh_timestamp );
  BOOST_CHECK_EQUAL( decision.wake_fetch_loop, wake_fetch_loop );
}

} // namespace

BOOST_AUTO_TEST_SUITE( p2p_advertised_item_tests )

/// The first advertisement of an item queues it and wakes the fetch loop.
BOOST_AUTO_TEST_CASE( new_item_is_queued )
{
  check_decision( decide_advertised_item_fetch( block_advertisement() ), true, false, true );

  advertised_item_status transaction = block_advertisement();
  transaction.is_block = false;
  check_decision( decide_advertised_item_fetch( transaction ), true, false, true );
}

/// A block whose copy is being pushed is not fetched a second time; its advertiser is only recorded, so a
/// refetch after the copy is rejected can ask it.
BOOST_AUTO_TEST_CASE( block_being_processed_is_not_fetched_again )
{
  advertised_item_status status = block_advertisement();
  status.being_processed = true;
  check_decision( decide_advertised_item_fetch( status ), false, false, false );

  status.queued = true;
  check_decision( decide_advertised_item_fetch( status ), false, false, false );
}

/// An item requested from a peer already, or recently failed, is neither queued nor wakes the loop.
BOOST_AUTO_TEST_CASE( requested_or_failed_item_is_left_alone )
{
  advertised_item_status requested = block_advertisement();
  requested.requested = true;
  check_decision( decide_advertised_item_fetch( requested ), false, false, false );

  advertised_item_status failed = block_advertisement();
  failed.recently_failed = true;
  check_decision( decide_advertised_item_fetch( failed ), false, false, false );
}

/// A block still waiting to be fetched wakes the fetch loop when a peer that hadn't advertised it does:
/// the iteration that queued it may have found no peer to ask.
BOOST_AUTO_TEST_CASE( new_source_of_queued_block_wakes_fetch_loop )
{
  advertised_item_status status = block_advertisement();
  status.queued = true;
  check_decision( decide_advertised_item_fetch( status ), false, true, true );
}

/// A repeated advertisement from the same peer only refreshes the timestamp, so duplicates don't churn the loop.
BOOST_AUTO_TEST_CASE( repeated_advertisement_does_not_wake_fetch_loop )
{
  advertised_item_status status = block_advertisement();
  status.queued = true;
  status.new_from_this_peer = false;
  check_decision( decide_advertised_item_fetch( status ), false, true, false );
}

/// Transaction fetching is unchanged: a queued transaction's new advertiser doesn't wake the loop.
BOOST_AUTO_TEST_CASE( new_source_of_queued_transaction_does_not_wake_fetch_loop )
{
  advertised_item_status status = block_advertisement();
  status.is_block = false;
  status.queued = true;
  check_decision( decide_advertised_item_fetch( status ), false, true, false );
}

BOOST_AUTO_TEST_SUITE_END()
