/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 */

#define BOOST_TEST_MODULE message_serialization_tests
#include <boost/test/unit_test.hpp>

#include <graphene/net/core_messages.hpp>
#include <graphene/net/message.hpp>
#include <graphene/net/config.hpp>
#include <fc/io/raw.hpp>
#include <fc/network/ip.hpp>

using namespace graphene::net;

// Helper function to convert fc::ip::address to string for comparison
static std::string addr_to_string(const fc::ip::address& addr)
{
  return static_cast<std::string>(addr);
}

BOOST_AUTO_TEST_SUITE(message_serialization_tests)

// Test that hello_message with IPv4 can be correctly unpacked using auto-detection
BOOST_AUTO_TEST_CASE(hello_message_ipv4_new_format)
{
  hello_message original;
  original.user_agent = "test_agent";
  original.core_protocol_version = GRAPHENE_NET_PROTOCOL_IPV6_VERSION; // v108
  original.inbound_address = fc::ip::address("192.168.1.100");
  original.inbound_port = 2001;
  original.outbound_port = 2002;
  // Note: other fields use default values

  // Serialize using new format (standard pack)
  std::vector<char> data = fc::raw::pack_to_vector(original);

  // Unpack using auto-detection
  hello_message unpacked = unpack_hello_message(data);

  BOOST_CHECK_EQUAL(unpacked.user_agent, original.user_agent);
  BOOST_CHECK_EQUAL(unpacked.core_protocol_version, original.core_protocol_version);
  BOOST_CHECK(unpacked.inbound_address.is_ipv4());
  BOOST_CHECK_EQUAL(addr_to_string(unpacked.inbound_address), "192.168.1.100");
  BOOST_CHECK_EQUAL(unpacked.inbound_port, original.inbound_port);
  BOOST_CHECK_EQUAL(unpacked.outbound_port, original.outbound_port);
}

// Test that hello_message with IPv6 can be correctly unpacked
BOOST_AUTO_TEST_CASE(hello_message_ipv6_new_format)
{
  hello_message original;
  original.user_agent = "test_agent_v6";
  original.core_protocol_version = GRAPHENE_NET_PROTOCOL_IPV6_VERSION;
  original.inbound_address = fc::ip::address("2001:db8::1");
  original.inbound_port = 3001;
  original.outbound_port = 3002;

  // Serialize using new format
  std::vector<char> data = fc::raw::pack_to_vector(original);

  // Unpack using auto-detection
  hello_message unpacked = unpack_hello_message(data);

  BOOST_CHECK_EQUAL(unpacked.user_agent, original.user_agent);
  BOOST_CHECK_EQUAL(unpacked.core_protocol_version, original.core_protocol_version);
  BOOST_CHECK(unpacked.inbound_address.is_ipv6());
  BOOST_CHECK_EQUAL(addr_to_string(unpacked.inbound_address), "2001:db8::1");
  BOOST_CHECK_EQUAL(unpacked.inbound_port, original.inbound_port);
  BOOST_CHECK_EQUAL(unpacked.outbound_port, original.outbound_port);
}

// Test that hello_message in legacy format (v107) can be correctly unpacked
BOOST_AUTO_TEST_CASE(hello_message_legacy_format)
{
  // Create a legacy-format hello message manually
  hello_message original;
  original.user_agent = "old_client";
  original.core_protocol_version = 107; // v107 - legacy
  original.inbound_address = fc::ip::address("10.0.0.1");
  original.inbound_port = 4001;
  original.outbound_port = 4002;

  // Pack in legacy format (simulating v107 node)
  // Legacy format: string, uint32, uint32 (IP), uint16, uint16, ...
  std::vector<char> data;
  fc::datastream<size_t> ps;
  fc::raw::pack(ps, original.user_agent);
  fc::raw::pack(ps, original.core_protocol_version);
  fc::raw::pack_legacy(ps, original.inbound_address); // Legacy IP format
  fc::raw::pack(ps, original.inbound_port);
  fc::raw::pack(ps, original.outbound_port);
  fc::raw::pack(ps, original.node_public_key);
  fc::raw::pack(ps, original.signed_shared_secret);
  fc::raw::pack(ps, original.user_data);
  data.resize(ps.tellp());

  fc::datastream<char*> ds(data.data(), data.size());
  fc::raw::pack(ds, original.user_agent);
  fc::raw::pack(ds, original.core_protocol_version);
  fc::raw::pack_legacy(ds, original.inbound_address);
  fc::raw::pack(ds, original.inbound_port);
  fc::raw::pack(ds, original.outbound_port);
  fc::raw::pack(ds, original.node_public_key);
  fc::raw::pack(ds, original.signed_shared_secret);
  fc::raw::pack(ds, original.user_data);

  // Unpack using auto-detection - should detect v107 and use legacy format
  hello_message unpacked = unpack_hello_message(data);

  BOOST_CHECK_EQUAL(unpacked.user_agent, original.user_agent);
  BOOST_CHECK_EQUAL(unpacked.core_protocol_version, 107u);
  BOOST_CHECK(unpacked.inbound_address.is_ipv4());
  BOOST_CHECK_EQUAL(addr_to_string(unpacked.inbound_address), "10.0.0.1");
  BOOST_CHECK_EQUAL(unpacked.inbound_port, original.inbound_port);
  BOOST_CHECK_EQUAL(unpacked.outbound_port, original.outbound_port);
}

// Test create_hello_message_for_peer with IPv6 support
BOOST_AUTO_TEST_CASE(create_hello_message_ipv6_peer)
{
  hello_message original;
  original.user_agent = "test_client";
  original.core_protocol_version = GRAPHENE_NET_PROTOCOL_VERSION;
  original.inbound_address = fc::ip::address("192.168.1.1");
  original.inbound_port = 5001;
  original.outbound_port = 5002;

  // Create message for IPv6-capable peer
  message msg = create_hello_message_for_peer(original, true);

  BOOST_CHECK_EQUAL(msg.msg_type, hello_message::type);
  BOOST_CHECK_GT(msg.size, 0u);

  // Unpack and verify
  hello_message unpacked = unpack_hello_message(msg.data);
  BOOST_CHECK_EQUAL(unpacked.user_agent, original.user_agent);
  BOOST_CHECK_EQUAL(addr_to_string(unpacked.inbound_address), "192.168.1.1");
}

// Test create_hello_message_for_peer for legacy peer
// Note: When sending to a legacy peer, we send our v108 message in legacy IP format.
// The legacy peer will parse it using their old unpacker which ignores the version
// for IP format. Our unpacker uses version to determine format, so this test verifies
// the raw format is correct by comparing sizes.
BOOST_AUTO_TEST_CASE(create_hello_message_legacy_peer)
{
  hello_message original;
  original.user_agent = "test_client";
  original.core_protocol_version = GRAPHENE_NET_PROTOCOL_VERSION;
  original.inbound_address = fc::ip::address("192.168.1.1");
  original.inbound_port = 5001;
  original.outbound_port = 5002;

  // Create message for IPv6-capable peer (new format)
  message msg_new = create_hello_message_for_peer(original, true);

  // Create message for legacy peer (legacy IP format)
  message msg_legacy = create_hello_message_for_peer(original, false);

  BOOST_CHECK_EQUAL(msg_legacy.msg_type, hello_message::type);
  BOOST_CHECK_GT(msg_legacy.size, 0u);

  // Legacy format should be 1 byte smaller for IPv4 (4 bytes vs 1+4 bytes)
  BOOST_CHECK_EQUAL(msg_new.size, msg_legacy.size + 1);

  // Verify the new format message can be unpacked correctly by our unpacker
  hello_message unpacked = unpack_hello_message(msg_new.data);
  BOOST_CHECK_EQUAL(unpacked.user_agent, original.user_agent);
  BOOST_CHECK_EQUAL(addr_to_string(unpacked.inbound_address), "192.168.1.1");
}

// Test address_message serialization for IPv6-capable peer
BOOST_AUTO_TEST_CASE(address_message_ipv6_peer)
{
  address_message original;
  original.addresses.push_back(address_info(
    fc::ip::endpoint(fc::ip::address("192.168.1.1"), 2001),
    fc::time_point_sec(),
    fc::microseconds(100),
    node_id_t(),
    peer_connection_direction::outbound,
    firewalled_state::not_firewalled
  ));
  original.addresses.push_back(address_info(
    fc::ip::endpoint(fc::ip::address("2001:db8::1"), 2001),
    fc::time_point_sec(),
    fc::microseconds(200),
    node_id_t(),
    peer_connection_direction::inbound,
    firewalled_state::unknown
  ));

  // Create message for IPv6-capable peer
  message msg = create_address_message_for_peer(original, true);

  BOOST_CHECK_EQUAL(msg.msg_type, address_message::type);

  // Unpack and verify - should have both addresses
  address_message unpacked = unpack_address_message(msg.data, true);
  BOOST_CHECK_EQUAL(unpacked.addresses.size(), 2u);
  BOOST_CHECK_EQUAL(addr_to_string(unpacked.addresses[0].remote_endpoint.get_address()), "192.168.1.1");
  BOOST_CHECK_EQUAL(addr_to_string(unpacked.addresses[1].remote_endpoint.get_address()), "2001:db8::1");
}

// Test address_message serialization for legacy peer - IPv6 addresses should be filtered
BOOST_AUTO_TEST_CASE(address_message_legacy_peer_filters_ipv6)
{
  address_message original;
  original.addresses.push_back(address_info(
    fc::ip::endpoint(fc::ip::address("192.168.1.1"), 2001),
    fc::time_point_sec(),
    fc::microseconds(100),
    node_id_t(),
    peer_connection_direction::outbound,
    firewalled_state::not_firewalled
  ));
  original.addresses.push_back(address_info(
    fc::ip::endpoint(fc::ip::address("2001:db8::1"), 2001),
    fc::time_point_sec(),
    fc::microseconds(200),
    node_id_t(),
    peer_connection_direction::inbound,
    firewalled_state::unknown
  ));
  original.addresses.push_back(address_info(
    fc::ip::endpoint(fc::ip::address("10.0.0.1"), 2002),
    fc::time_point_sec(),
    fc::microseconds(300),
    node_id_t(),
    peer_connection_direction::outbound,
    firewalled_state::firewalled
  ));

  // Create message for legacy peer (doesn't support IPv6)
  message msg = create_address_message_for_peer(original, false);

  BOOST_CHECK_EQUAL(msg.msg_type, address_message::type);

  // Unpack as legacy - should only have IPv4 addresses (2 out of 3)
  address_message unpacked = unpack_address_message(msg.data, false);
  BOOST_CHECK_EQUAL(unpacked.addresses.size(), 2u);
  BOOST_CHECK_EQUAL(addr_to_string(unpacked.addresses[0].remote_endpoint.get_address()), "192.168.1.1");
  BOOST_CHECK_EQUAL(addr_to_string(unpacked.addresses[1].remote_endpoint.get_address()), "10.0.0.1");
}

// Test connection_rejected_message serialization
BOOST_AUTO_TEST_CASE(connection_rejected_message_ipv4)
{
  connection_rejected_message original;
  original.user_agent = "rejecting_node";
  original.core_protocol_version = GRAPHENE_NET_PROTOCOL_VERSION;
  original.remote_endpoint = fc::ip::endpoint(fc::ip::address("192.168.1.50"), 3000);
  original.reason_code = rejection_reason_code::not_accepting_connections;
  original.reason_string = "Server full";

  // Test with IPv6-capable peer
  message msg = create_connection_rejected_message_for_peer(original, true);
  BOOST_CHECK_EQUAL(msg.msg_type, connection_rejected_message::type);

  connection_rejected_message unpacked = unpack_connection_rejected_message(msg.data, true);
  BOOST_CHECK_EQUAL(unpacked.user_agent, original.user_agent);
  BOOST_CHECK_EQUAL(addr_to_string(unpacked.remote_endpoint.get_address()), "192.168.1.50");
  BOOST_CHECK_EQUAL(unpacked.remote_endpoint.port(), 3000);
  BOOST_CHECK(unpacked.reason_code == rejection_reason_code::not_accepting_connections);
}

// Test connection_rejected_message for legacy peer
BOOST_AUTO_TEST_CASE(connection_rejected_message_legacy)
{
  connection_rejected_message original;
  original.user_agent = "rejecting_node";
  original.core_protocol_version = GRAPHENE_NET_PROTOCOL_VERSION;
  original.remote_endpoint = fc::ip::endpoint(fc::ip::address("10.0.0.50"), 4000);
  original.reason_code = rejection_reason_code::already_connected;
  original.reason_string = "Already connected";

  // Test with legacy peer
  message msg = create_connection_rejected_message_for_peer(original, false);
  BOOST_CHECK_EQUAL(msg.msg_type, connection_rejected_message::type);

  connection_rejected_message unpacked = unpack_connection_rejected_message(msg.data, false);
  BOOST_CHECK_EQUAL(unpacked.user_agent, original.user_agent);
  BOOST_CHECK_EQUAL(addr_to_string(unpacked.remote_endpoint.get_address()), "10.0.0.50");
  BOOST_CHECK_EQUAL(unpacked.remote_endpoint.port(), 4000);
}

// Test check_firewall_message serialization
BOOST_AUTO_TEST_CASE(check_firewall_message_serialization)
{
  check_firewall_message original;
  original.endpoint_to_check = fc::ip::endpoint(fc::ip::address("192.168.1.100"), 5000);

  // Test with IPv6-capable peer
  message msg = create_check_firewall_message_for_peer(original, true);
  BOOST_CHECK_EQUAL(msg.msg_type, check_firewall_message::type);

  check_firewall_message unpacked = unpack_check_firewall_message(msg.data, true);
  BOOST_CHECK_EQUAL(addr_to_string(unpacked.endpoint_to_check.get_address()), "192.168.1.100");
  BOOST_CHECK_EQUAL(unpacked.endpoint_to_check.port(), 5000);
}

// Test check_firewall_reply_message serialization
BOOST_AUTO_TEST_CASE(check_firewall_reply_message_serialization)
{
  check_firewall_reply_message original;
  original.endpoint_checked = fc::ip::endpoint(fc::ip::address("192.168.1.100"), 5000);
  original.result = firewall_check_result::connection_successful;

  // Test with IPv6-capable peer
  message msg = create_check_firewall_reply_message_for_peer(original, true);
  BOOST_CHECK_EQUAL(msg.msg_type, check_firewall_reply_message::type);

  check_firewall_reply_message unpacked = unpack_check_firewall_reply_message(msg.data, true);
  BOOST_CHECK_EQUAL(addr_to_string(unpacked.endpoint_checked.get_address()), "192.168.1.100");
  BOOST_CHECK_EQUAL(unpacked.endpoint_checked.port(), 5000);
  BOOST_CHECK(unpacked.result == firewall_check_result::connection_successful);
}

// Test that IPv6 firewall check works
BOOST_AUTO_TEST_CASE(check_firewall_message_ipv6)
{
  check_firewall_message original;
  original.endpoint_to_check = fc::ip::endpoint(fc::ip::address("2001:db8::100"), 6000);

  // Test with IPv6-capable peer
  message msg = create_check_firewall_message_for_peer(original, true);
  BOOST_CHECK_EQUAL(msg.msg_type, check_firewall_message::type);

  check_firewall_message unpacked = unpack_check_firewall_message(msg.data, true);
  BOOST_CHECK(unpacked.endpoint_to_check.get_address().is_ipv6());
  BOOST_CHECK_EQUAL(addr_to_string(unpacked.endpoint_to_check.get_address()), "2001:db8::100");
  BOOST_CHECK_EQUAL(unpacked.endpoint_to_check.port(), 6000);
}

BOOST_AUTO_TEST_SUITE_END()
