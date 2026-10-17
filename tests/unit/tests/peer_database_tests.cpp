#include <boost/test/unit_test.hpp>

#include <graphene/net/peer_database.hpp>

#include <hive/utilities/tempdir.hpp>

#include <fc/filesystem.hpp>

#include <fstream>

using graphene::net::peer_database;
using fc::ip::endpoint;

BOOST_AUTO_TEST_SUITE( peer_database_tests )

/// A peers.json record this build cannot convert (malformed, truncated, or written in a newer
/// format) must cost only that record: the readable records around it are still loaded.
BOOST_AUTO_TEST_CASE( unreadable_record_does_not_discard_the_others )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  const fc::path peers_file = dir.path() / "peers.json";
  {
    std::ofstream out( peers_file.string() );
    out << R"([
      {"endpoint":"192.168.1.10:2001","last_seen_time":"2026-01-01T00:00:00","last_connection_disposition":"last_connection_succeeded","last_connection_attempt_time":"2026-01-01T00:00:00","number_of_successful_connection_attempts":3,"number_of_failed_connection_attempts":0},
      {"endpoint":"not-an-endpoint","last_seen_time":"2026-01-01T00:00:00","last_connection_disposition":"never_attempted_to_connect","last_connection_attempt_time":"1970-01-01T00:00:00","number_of_successful_connection_attempts":0,"number_of_failed_connection_attempts":0},
      {"endpoint":"[2607:5300:203:1::1]:2001","last_seen_time":"2026-01-02T00:00:00","last_connection_disposition":"never_attempted_to_connect","last_connection_attempt_time":"1970-01-01T00:00:00","number_of_successful_connection_attempts":0,"number_of_failed_connection_attempts":0}
    ])";
  }

  peer_database db;
  db.open( peers_file );

  BOOST_REQUIRE_EQUAL( db.size(), 2u );

  const auto ipv4 = db.lookup_entry_for_endpoint( endpoint::from_string( "192.168.1.10:2001" ) );
  BOOST_REQUIRE( ipv4.valid() );
  BOOST_REQUIRE_EQUAL( ipv4->number_of_successful_connection_attempts, 3u );

  const auto ipv6 = db.lookup_entry_for_endpoint( endpoint::from_string( "[2607:5300:203:1::1]:2001" ) );
  BOOST_REQUIRE( ipv6.valid() );
  BOOST_REQUIRE( ipv6->last_seen_time == fc::time_point_sec::from_iso_string( "2026-01-02T00:00:00" ) );
}

BOOST_AUTO_TEST_SUITE_END()
