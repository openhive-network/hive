#pragma once

#include <utility>
#include <string>

#include <hive/protocol/types.hpp>
#include <hive/protocol/config.hpp>

#include <fc/crypto/ripemd160.hpp>

namespace hive { namespace converter { namespace plugins { namespace iceberg_generate { namespace detail {

  using hive::protocol::account_name_type;

  using ops_permlink_tracker_result_t = std::pair<account_name_type, std::string>;

  using author_and_permlink_hash_t = fc::ripemd160;

  // Similar function is defined in the comment_object header, but it generates hash from the account_object_id, which we do not have in the iceberg plugin
  author_and_permlink_hash_t compute_author_and_permlink_hash( const account_name_type& author, const std::string& permlink )
  {
    return fc::ripemd160::hash( permlink + "@" + author );
  }

  author_and_permlink_hash_t compute_author_and_permlink_hash( const ops_permlink_tracker_result_t& plink_visitor_result )
  {
    return compute_author_and_permlink_hash( plink_visitor_result.first, plink_visitor_result.second );
  }

  class created_permlinks_visitor
  {
  public:
    typedef ops_permlink_tracker_result_t result_type;

    result_type operator()( hive::protocol::comment_operation& op )const
    {
      return { op.author, op.permlink };
    }

    result_type operator()( hive::protocol::create_proposal_operation& op )const
    {
      return { op.creator, op.permlink };
    }

    // No signatures modification ops
    template< typename T >
    result_type operator()( const T& op )const
    {
      FC_ASSERT( !op.is_virtual(), "block log should not contain virtual operations" );
      return { "", "" };
    }
  };

  class dependent_permlinks_visitor
  {
  public:
    typedef ops_permlink_tracker_result_t result_type;

    result_type operator()( hive::protocol::comment_operation& op )const
    {
      if( op.parent_author != HIVE_ROOT_POST_PARENT )
        return { op.parent_author, op.parent_permlink };

      // This is usually not a case, but in the hive_evaluator it is allowed for parent_permlink to exist (e.g. firstpost),
      // but when the parent_author is set to HIVE_ROOT_POST_PARENT, the parent_permlink value is ignored, so we can't
      // return it, because there may be something there, and instead we have to return an empty string.
      return { HIVE_ROOT_POST_PARENT, "" };
    }

    result_type operator()( hive::protocol::comment_options_operation& op )const
    {
      return { op.author, op.permlink };
    }

    result_type operator()( hive::protocol::vote_operation& op )const
    {
      return { op.author, op.permlink };
    }

    result_type operator()( hive::protocol::update_proposal_operation& op )const
    {
      return { op.creator, op.permlink };
    }

    // No signatures modification ops
    template< typename T >
    result_type operator()( const T& op )const
    {
      FC_ASSERT( !op.is_virtual(), "block log should not contain virtual operations" );
      return { "", "" };
    }
  };

} } } } }
