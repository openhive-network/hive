#pragma once

#include <hive/protocol/types.hpp>

namespace hive { namespace converter { namespace plugins { namespace iceberg_generate { namespace detail {

  class ops_strip_content_visitor
  {
  public:
    typedef hive::protocol::operation result_type;

    result_type operator()( hive::protocol::account_create_operation& op )const
    {
      op.owner.clear();
      op.active.clear();
      op.posting.clear();
      op.json_metadata.clear();

      return op;
    }

    result_type operator()( hive::protocol::account_create_with_delegation_operation& op )const
    {
      op.owner.clear();
      op.active.clear();
      op.posting.clear();
      op.json_metadata.clear();
      op.extensions.clear();

      return op;
    }

    result_type operator()( hive::protocol::account_update_operation& op )const
    {
      op.owner.reset();
      op.active.reset();
      op.posting.reset();
      op.json_metadata.clear();

      return op;
    }

    result_type operator()( hive::protocol::account_update2_operation& op )const
    {
      op.owner.reset();
      op.active.reset();
      op.posting.reset();
      op.memo_key.reset();
      op.json_metadata.clear();
      op.posting_json_metadata.clear();
      op.extensions.clear();

      return op;
    }

    result_type operator()( hive::protocol::comment_operation& op )const
    {
      op.title.clear();
      op.body.clear();
      op.json_metadata.clear();

      return op;
    }

    result_type operator()( hive::protocol::create_claimed_account_operation& op )const
    {
      op.owner.clear();
      op.active.clear();
      op.posting.clear();
      op.json_metadata.clear();
      op.extensions.clear();

      return op;
    }

    result_type operator()( hive::protocol::transfer_operation& op )const
    {
      op.memo.clear();

      return op;
    }

    result_type operator()( hive::protocol::escrow_transfer_operation& op )const
    {
      op.json_meta.clear();

      return op;
    }

    result_type operator()( hive::protocol::witness_update_operation& op )const
    {
      op.url.clear();

      return op;
    }

    result_type operator()( hive::protocol::witness_set_properties_operation& op )const
    {
      op.extensions.clear();

      return op;
    }

    result_type operator()( hive::protocol::custom_operation& op )const
    {
      op.required_auths.clear();
      op.data.clear();

      return op;
    }

    result_type operator()( hive::protocol::custom_json_operation& op )const
    {
      op.required_auths.clear();
      op.required_posting_auths.clear();
      op.json.clear();

      return op;
    }

    result_type operator()( hive::protocol::custom_binary_operation& op )const
    {
      op.required_owner_auths.clear();
      op.required_active_auths.clear();
      op.required_posting_auths.clear();
      op.required_auths.clear();
      op.data.clear();

      return op;
    }

    result_type operator()( hive::protocol::request_account_recovery_operation& op )const
    {
      op.new_owner_authority.clear();
      op.extensions.clear();

      return op;
    }

    result_type operator()( hive::protocol::recover_account_operation& op )const
    {
      op.new_owner_authority.clear();
      op.recent_owner_authority.clear();
      op.extensions.clear();

      return op;
    }

    result_type operator()( hive::protocol::reset_account_operation& op )const
    {
      op.new_owner_authority.clear();

      return op;
    }

    result_type operator()( hive::protocol::change_recovery_account_operation& op )const
    {
      op.extensions.clear();

      return op;
    }

    result_type operator()( hive::protocol::transfer_to_savings_operation& op )const
    {
      op.memo.clear();

      return op;
    }

    result_type operator()( hive::protocol::transfer_from_savings_operation& op )const
    {
      op.memo.clear();

      return op;
    }

    result_type operator()( hive::protocol::recurrent_transfer_operation& op )const
    {
      op.memo.clear();
      op.extensions.clear();

      return op;
    }

    result_type operator()( hive::protocol::create_proposal_operation& op )const
    {
      op.subject.clear();
      op.extensions.clear();

      return op;
    }

    result_type operator()( hive::protocol::update_proposal_operation& op )const
    {
      op.subject.clear();
      op.extensions.clear();

      return op;
    }

    result_type operator()( hive::protocol::update_proposal_votes_operation& op )const
    {
      op.extensions.clear();

      return op;
    }

    result_type operator()( hive::protocol::remove_proposal_operation& op )const
    {
      op.extensions.clear();

      return op;
    }

    // No signatures modification ops
    template< typename T >
    result_type operator()( const T& op )const
    {
      FC_ASSERT( !op.is_virtual(), "block log should not contain virtual operations" );
      return op;
    }
  };

} } } } }
