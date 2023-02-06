#pragma once

#include <vector>
#include <string>

#include <fc/optional.hpp>

namespace hive { namespace converter { namespace plugins { namespace iceberg_generate { namespace detail {

  class ops_permlink_tracker_visitor
  {
  public:
    typedef std::vector<std::string> result_type;

    result_type operator()( hp::comment_operation& op )const
    {
      return { op.permlink, op.parent_permlink };
    }

    result_type operator()( hp::comment_options_operation& op )const
    {
      return { op.permlink };
    }

    result_type operator()( hp::vote_operation& op )const
    {
      return { op.permlink };
    }

    result_type operator()( hp::create_proposal_operation& op )const
    {
      return { op.permlink };
    }

    result_type operator()( hp::update_proposal_operation& op )const
    {
      return { op.permlink };
    }

    // No signatures modification ops
    template< typename T >
    result_type operator()( const T& op )const
    {
      FC_ASSERT( !op.is_virtual(), "block log should not contain virtual operations" );
      return {};
    }
  };

} } } } }
