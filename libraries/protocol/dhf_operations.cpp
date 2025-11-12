#include <hive/protocol/dhf_operations.hpp>

#include <hive/protocol/validation.hpp>

namespace hive { namespace protocol {

void create_proposal_operation::validate()const
{
  validate_account_name( creator );
  validate_account_name( receiver );

  HIVE_PROTOCOL_DHF_OPERATIONS_ASSERT( end_date > start_date, "end date must be greater than start date" );

  validate_asset_not_negative(daily_pay, "Daily pay can't be negative value", *this);
  validate_asset_type(daily_pay, HBD_SYMBOL, "Daily pay should be expressed in HBD", *this);

  validate_string_is_not_empty(subject, "subject is required", *this);
  validate_string_max_size(subject, HIVE_PROPOSAL_SUBJECT_MAX_LENGTH, "Subject is too long", *this);
  validate_is_string_in_utf8(subject, "Subject is not valid UTF8", *this);

  validate_string_is_not_empty(permlink, "permlink is required", *this);
  validate_permlink(permlink);
}

void update_proposal_operation::validate()const
{
  validate_account_name( creator );
  HIVE_PROTOCOL_DHF_OPERATIONS_ASSERT( proposal_id >= 0, "The proposal id can't be negative" );
  validate_asset_not_negative(daily_pay, "Daily pay can't be negative value", *this);
  validate_asset_type(daily_pay, HBD_SYMBOL, "Daily pay should be expressed in HBD", *this);

  validate_string_is_not_empty( subject, "subject is required", *this );
  validate_string_max_size(subject, HIVE_PROPOSAL_SUBJECT_MAX_LENGTH, "Subject is too long", *this);
  validate_is_string_in_utf8(subject, "Subject is not valid UTF8", *this);
  
  validate_string_is_not_empty( permlink, "permlink is required", *this );
  validate_permlink(permlink);
}

void update_proposal_votes_operation::validate()const
{
  validate_account_name( voter );
  HIVE_PROTOCOL_DHF_OPERATIONS_ASSERT(
    proposal_ids.empty() == false && "update_proposal_votes_operation", 
    "Proposal id must be provided", 
    ("subject", *this)
  );
  HIVE_PROTOCOL_DHF_OPERATIONS_ASSERT(proposal_ids.size() <= HIVE_PROPOSAL_MAX_IDS_NUMBER, "Too many ids");
}

void remove_proposal_operation::validate() const
{
  validate_account_name(proposal_owner);
  HIVE_PROTOCOL_DHF_OPERATIONS_ASSERT(
    proposal_ids.empty() == false && "remove_proposal_operation", 
    "Proposal id must be provided", 
    ("subject", *this)
  );
  HIVE_PROTOCOL_DHF_OPERATIONS_ASSERT(proposal_ids.size() <= HIVE_PROPOSAL_MAX_IDS_NUMBER && "Too many ids");
}

} } //hive::protocol
