#include <boost/test/unit_test.hpp>

#include <graphene/net/config.hpp>
#include <graphene/net/node.hpp>

#include <cstdint>
#include <limits>

using graphene::net::peer_needs_blocks_we_pruned;

namespace {

// tail block of a node without block log (block-log-split = 0) at the head of mainnet
const uint32_t pruned_tail_block_num = 111228968;

} // namespace

BOOST_AUTO_TEST_SUITE( p2p_pruned_peer_tests )

/// A node that keeps the whole block log can serve any peer, however far behind.
BOOST_AUTO_TEST_CASE( unpruned_node_serves_every_peer )
{
  BOOST_CHECK( !peer_needs_blocks_we_pruned( 0, 1 ) );
  BOOST_CHECK( !peer_needs_blocks_we_pruned( 1, 1 ) );
  BOOST_CHECK( !peer_needs_blocks_we_pruned( pruned_tail_block_num, 1 ) );
}

/// A peer without blocks, or one that was offline for hours, can't sync from a node that pruned
/// the blocks it needs.
BOOST_AUTO_TEST_CASE( peer_far_behind_tail_needs_pruned_blocks )
{
  BOOST_CHECK( peer_needs_blocks_we_pruned( 0, pruned_tail_block_num ) );
  BOOST_CHECK( peer_needs_blocks_we_pruned( pruned_tail_block_num - 17520, pruned_tail_block_num ) );
  BOOST_CHECK( peer_needs_blocks_we_pruned( pruned_tail_block_num - GRAPHENE_NET_PRUNED_TAIL_GRACE_BLOCKS - 1,
                                            pruned_tail_block_num ) );
}

/// With one-block irreversibility the tail of a node without block log is its head block or close
/// to it, so a peer in live sync that is momentarily a few blocks behind must still be served.
BOOST_AUTO_TEST_CASE( peer_slightly_behind_tail_is_served )
{
  BOOST_CHECK( !peer_needs_blocks_we_pruned( pruned_tail_block_num - GRAPHENE_NET_PRUNED_TAIL_GRACE_BLOCKS,
                                             pruned_tail_block_num ) );
  BOOST_CHECK( !peer_needs_blocks_we_pruned( pruned_tail_block_num - 1, pruned_tail_block_num ) );
  BOOST_CHECK( !peer_needs_blocks_we_pruned( pruned_tail_block_num, pruned_tail_block_num ) );
  BOOST_CHECK( !peer_needs_blocks_we_pruned( pruned_tail_block_num + 1000, pruned_tail_block_num ) );
}

/// On a chain younger than the grace margin a peer without blocks is not caught by this check;
/// the block id reader refuses to make up ids for it instead (see block_storage_tests).
BOOST_AUTO_TEST_CASE( young_chain_is_left_to_block_id_reader )
{
  BOOST_CHECK( !peer_needs_blocks_we_pruned( 0, GRAPHENE_NET_PRUNED_TAIL_GRACE_BLOCKS ) );
  BOOST_CHECK( peer_needs_blocks_we_pruned( 0, GRAPHENE_NET_PRUNED_TAIL_GRACE_BLOCKS + 1 ) );
}

/// A bogus head block number reported by a peer must not wrap around into "far behind".
BOOST_AUTO_TEST_CASE( huge_peer_head_does_not_overflow )
{
  BOOST_CHECK( !peer_needs_blocks_we_pruned( std::numeric_limits<uint32_t>::max(), pruned_tail_block_num ) );
}

BOOST_AUTO_TEST_SUITE_END()
