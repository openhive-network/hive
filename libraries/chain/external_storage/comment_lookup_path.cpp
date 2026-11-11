#include <hive/chain/external_storage/comment_lookup_path.hpp>

namespace hive { namespace chain {

namespace {

using namespace hive::protocol;

using lookup_path = hive::utilities::benchmark_dumper::comment_lookup_path;

struct evaluator_lookup_path_visitor
{
  typedef lookup_path result_type;

  lookup_path operator()( const vote_operation& ) const { return lookup_path::vote_evaluator; }
  lookup_path operator()( const comment_operation& ) const { return lookup_path::comment_evaluator; }
  lookup_path operator()( const comment_options_operation& ) const { return lookup_path::comment_options_evaluator; }
  lookup_path operator()( const delete_comment_operation& ) const { return lookup_path::delete_comment_evaluator; }
  lookup_path operator()( const create_proposal_operation& ) const { return lookup_path::proposal_evaluator; }
  lookup_path operator()( const update_proposal_operation& ) const { return lookup_path::proposal_evaluator; }

  template< typename T >
  lookup_path operator()( const T& ) const { return lookup_path::other_evaluator; }
};

}

hive::utilities::benchmark_dumper::comment_lookup_path get_evaluator_lookup_path( const hive::protocol::operation& op )
{
  return op.visit( evaluator_lookup_path_visitor() );
}

} } // hive::chain
