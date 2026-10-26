#include <boost/test/unit_test.hpp>

#include <graphene/net/peer_database.hpp>

#include <hive/utilities/tempdir.hpp>

#include <fc/filesystem.hpp>
#include <fc/io/json.hpp>
#include <fc/log/appender.hpp>
#include <fc/log/logger.hpp>

#include <boost/algorithm/string/predicate.hpp>

#include <fstream>
#include <string>
#include <vector>

using graphene::net::peer_database;
using graphene::net::potential_peer_record;
using fc::ip::endpoint;

namespace {

class capturing_appender : public fc::appender
{
public:
  std::vector<std::string> messages;

  void log( const fc::log_message& m ) override { messages.push_back( m.get_message() ); }
};

/// Collects what peer_database logs while in scope, so a test can check what an operator would see.
class log_capture
{
public:
  log_capture() : _logger( fc::logger::get() ), _previous_level( _logger.get_log_level() ), _appender( new capturing_appender )
  {
    _logger.set_log_level( fc::log_level::all );
    _logger.add_appender( _appender );
  }

  ~log_capture()
  {
    _logger.remove_appender( _appender );
    _logger.set_log_level( _previous_level );
  }

  const std::vector<std::string>& messages() const { return static_cast<const capturing_appender&>( *_appender ).messages; }

  std::vector<std::string> messages_containing( const std::string& text ) const
  {
    std::vector<std::string> result;
    for( const std::string& m : messages() )
      if( boost::algorithm::contains( m, text ) )
        result.push_back( m );
    return result;
  }

private:
  fc::logger _logger;
  fc::log_level _previous_level;
  fc::shared_ptr<fc::appender> _appender;
};

const char* const clean_database_text = "starting with a clean database";

void write_file( const fc::path& file, const std::string& content )
{
  std::ofstream out( file.string() );
  out << content;
}

const char* const own_and_other_peers_json = R"([
      {"endpoint":"[fd5e:6::30]:2001","last_seen_time":"2026-01-01T00:00:00","last_connection_disposition":"never_attempted_to_connect","last_connection_attempt_time":"1970-01-01T00:00:00","number_of_successful_connection_attempts":0,"number_of_failed_connection_attempts":0},
      {"endpoint":"172.30.66.30:2001","last_seen_time":"2026-01-01T00:00:00","last_connection_disposition":"never_attempted_to_connect","last_connection_attempt_time":"1970-01-01T00:00:00","number_of_successful_connection_attempts":0,"number_of_failed_connection_attempts":0},
      {"endpoint":"172.30.66.20:2001","last_seen_time":"2026-01-01T00:00:00","last_connection_disposition":"last_connection_succeeded","last_connection_attempt_time":"2026-01-01T00:00:00","number_of_successful_connection_attempts":1,"number_of_failed_connection_attempts":0}
    ])";

std::vector<endpoint> endpoints_in_file( const fc::path& file )
{
  const fc::variants records = fc::json::from_file( file, fc::json::format_validation_mode::full ).get_array();
  std::vector<endpoint> result;
  for( const fc::variant& record : records )
    result.push_back( record.as<potential_peer_record>().endpoint );
  return result;
}

} // namespace

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

  log_capture log;
  peer_database db;
  db.open( peers_file );

  BOOST_REQUIRE_EQUAL( db.size(), 2u );

  BOOST_REQUIRE( log.messages_containing( clean_database_text ).empty() );
  const auto skipped = log.messages_containing( "skipping unreadable record" );
  BOOST_REQUIRE_EQUAL( skipped.size(), 1u );
  BOOST_REQUIRE( boost::algorithm::contains( skipped.front(), "not-an-endpoint" ) );

  const auto ipv4 = db.lookup_entry_for_endpoint( endpoint::from_string( "192.168.1.10:2001" ) );
  BOOST_REQUIRE( ipv4.valid() );
  BOOST_REQUIRE_EQUAL( ipv4->number_of_successful_connection_attempts, 3u );

  const auto ipv6 = db.lookup_entry_for_endpoint( endpoint::from_string( "[2607:5300:203:1::1]:2001" ) );
  BOOST_REQUIRE( ipv6.valid() );
  BOOST_REQUIRE( ipv6->last_seen_time == fc::time_point_sec::from_iso_string( "2026-01-02T00:00:00" ) );
}

/// When peers.json cannot be read as a whole the node starts with an empty database, and the log line
/// must say which file it was and why, so a truncated file can be told apart from other failures.
BOOST_AUTO_TEST_CASE( truncated_file_logs_the_parse_error )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  const fc::path peers_file = dir.path() / "peers.json";
  write_file( peers_file, R"([
      {"endpoint":"192.168.1.10:2001","last_seen_time":"2026-01-01T00:00:00","last_connection_disposition":"last_conn)" );

  log_capture log;
  peer_database db;
  db.open( peers_file );

  BOOST_REQUIRE_EQUAL( db.size(), 0u );
  const auto errors = log.messages_containing( clean_database_text );
  BOOST_REQUIRE_EQUAL( errors.size(), 1u );
  BOOST_TEST_MESSAGE( errors.front() );
  BOOST_REQUIRE( boost::algorithm::contains( errors.front(), peers_file.generic_string() ) );
  BOOST_REQUIRE( boost::algorithm::contains( errors.front(), "EOF" ) );
}

BOOST_AUTO_TEST_CASE( non_array_file_logs_the_type_error )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  const fc::path peers_file = dir.path() / "peers.json";
  write_file( peers_file, R"({"endpoint":"192.168.1.10:2001"})" );

  log_capture log;
  peer_database db;
  db.open( peers_file );

  BOOST_REQUIRE_EQUAL( db.size(), 0u );
  const auto errors = log.messages_containing( clean_database_text );
  BOOST_REQUIRE_EQUAL( errors.size(), 1u );
  BOOST_TEST_MESSAGE( errors.front() );
  BOOST_REQUIRE( boost::algorithm::contains( errors.front(), peers_file.generic_string() ) );
  BOOST_REQUIRE( boost::algorithm::contains( errors.front(), "Invalid cast from object_type to Array" ) );
}

/// Peers advertise this node's own address back to it; storing it would make the node dial itself.
BOOST_AUTO_TEST_CASE( own_endpoint_is_never_inserted )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  peer_database db;
  db.open( dir.path() / "peers.json" );

  const endpoint own = endpoint::from_string( "[fd5e:6::30]:2001" );
  const endpoint same_host_other_port = endpoint::from_string( "[fd5e:6::30]:2002" );
  BOOST_REQUIRE( db.add_own_endpoint( own ) );
  BOOST_REQUIRE( !db.add_own_endpoint( own ) );
  BOOST_REQUIRE( db.is_own_endpoint( own ) );
  BOOST_REQUIRE( !db.is_own_endpoint( same_host_other_port ) );

  db.update_entry( db.lookup_or_create_entry_for_endpoint( own ) );
  db.update_entry( db.lookup_or_create_entry_for_endpoint( same_host_other_port ) );

  BOOST_REQUIRE_EQUAL( db.size(), 1u );
  BOOST_REQUIRE( !db.lookup_entry_for_endpoint( own ).valid() );
  BOOST_REQUIRE( db.lookup_entry_for_endpoint( same_host_other_port ).valid() );
  db.close();
}

/// An own endpoint learned while the node runs is removed from the entries already loaded, so
/// peers.json written at shutdown no longer contains it.
BOOST_AUTO_TEST_CASE( own_endpoint_learned_after_load_is_not_written )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  const fc::path peers_file = dir.path() / "peers.json";
  write_file( peers_file, own_and_other_peers_json );

  peer_database db;
  db.open( peers_file );
  BOOST_REQUIRE_EQUAL( db.size(), 3u );

  db.add_own_endpoint( endpoint::from_string( "[fd5e:6::30]:2001" ) );
  db.add_own_endpoint( endpoint::from_string( "172.30.66.30:2001" ) );
  BOOST_REQUIRE_EQUAL( db.size(), 1u );
  db.close();

  const std::vector<endpoint> written = endpoints_in_file( peers_file );
  BOOST_REQUIRE_EQUAL( written.size(), 1u );
  BOOST_REQUIRE( written.front() == endpoint::from_string( "172.30.66.20:2001" ) );
}

/// peers.json written by an older version may contain the node's own endpoints; known ones are
/// dropped on load and the cleaned database is what gets written back.
BOOST_AUTO_TEST_CASE( own_endpoint_persisted_by_older_version_is_dropped_on_load )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  const fc::path peers_file = dir.path() / "peers.json";
  write_file( peers_file, own_and_other_peers_json );

  peer_database db;
  db.add_own_endpoint( endpoint::from_string( "[fd5e:6::30]:2001" ) );
  db.add_own_endpoint( endpoint::from_string( "172.30.66.30:2001" ) );
  db.open( peers_file );

  BOOST_REQUIRE_EQUAL( db.size(), 1u );
  BOOST_REQUIRE( db.lookup_entry_for_endpoint( endpoint::from_string( "172.30.66.20:2001" ) ).valid() );
  db.close();

  const std::vector<endpoint> written = endpoints_in_file( peers_file );
  BOOST_REQUIRE_EQUAL( written.size(), 1u );
  BOOST_REQUIRE( written.front() == endpoint::from_string( "172.30.66.20:2001" ) );
}

BOOST_AUTO_TEST_SUITE_END()
