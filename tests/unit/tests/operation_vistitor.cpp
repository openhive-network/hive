#include <boost/test/unit_test.hpp>

#include <fc/variant.hpp>
#include <fc/variant_object.hpp>

#include <hive/protocol/operations.hpp>

#include <chrono>

BOOST_AUTO_TEST_SUITE( operation_visitor )

struct stub_visitor
{
  typedef void result_type;

  template<typename Type>
  result_type operator()( const Type& op )const { return;}
};

BOOST_AUTO_TEST_CASE( visit_performance ) {
  try {
    hive::protocol::operation operation_under_test;
    stub_visitor visitor;

    BOOST_TEST_MESSAGE( "Start of visit_performance test" );
    for ( auto type_id = 0; type_id < operation_under_test.count(); ++type_id)
    {
      operation_under_test.set_which(type_id);
      auto start = std::chrono::high_resolution_clock::now();
      for (auto j = 0; j < 100'000'000; j++) {
        operation_under_test.visit(visitor);
      }
      auto end = std::chrono::high_resolution_clock::now();
      BOOST_TEST_MESSAGE( "element " << type_id << " [ns] :" << std::chrono::duration_cast< std::chrono::nanoseconds >(end - start).count() );
    }
    BOOST_TEST_MESSAGE( "End of visit_performance test" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(bad_object) {
  bool thrown = false;
  bool verified = false;
  try {
    BOOST_TEST_MESSAGE("Start of static_variant type checks");

    hive::protocol::account_create_operation testOp;
    hive::protocol::operation operation_under_test(testOp);
    /// Ok
    operation_under_test.get<hive::protocol::account_create_operation>();
    verified = true;

    /// Intentially bad
    operation_under_test.get<hive::protocol::transfer_operation>();

  }
  catch(const fc::assert_exception& ae)
  {
    thrown = true;
    BOOST_TEST_MESSAGE("Caught assert exception: " + ae.to_string());
  }
  FC_LOG_AND_RETHROW()

  BOOST_CHECK(thrown);
  BOOST_CHECK(verified);

  BOOST_TEST_MESSAGE("End of static_variant type checks");
}

BOOST_AUTO_TEST_SUITE_END()
