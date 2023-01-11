#include <hive/chain/hive_object_types.hpp>
#include <hive/plugins/account_by_key/account_by_key_objects.hpp>
#include <hive/plugins/account_history_rocksdb/account_history_rocksdb_objects.hpp>
#include <hive/plugins/block_log_info/block_log_info_objects.hpp>
#include <hive/plugins/follow/follow_objects.hpp>
#include <hive/plugins/market_history/market_history_plugin.hpp>
#include <hive/plugins/rc/rc_objects.hpp>
#include <hive/plugins/reputation/reputation_objects.hpp>
#include <hive/plugins/tags/tags_plugin.hpp>
#include <hive/plugins/transaction_status/transaction_status_objects.hpp>
#include <hive/plugins/witness/witness_plugin_objects.hpp>
#include <hive/chain/block_summary_object.hpp>
#include <hive/chain/dhf_objects.hpp>
#include <hive/chain/pending_required_action_object.hpp>
#include <hive/chain/pending_optional_action_object.hpp>
#include <hive/chain/transaction_object.hpp>

#ifdef HIVE_ENABLE_SMT

#include <hive/chain/smt_objects/smt_token_object.hpp>
#include <hive/chain/smt_objects/account_balance_object.hpp>
#include <hive/chain/smt_objects/nai_pool_object.hpp>
#include <hive/chain/smt_objects/smt_token_object.hpp>

#endif

#include <iostream>
#include <sstream>
#include <unordered_map>
#include <chrono>

struct member_data
{
  std::string type_or_value;
  std::string name;
};

class decoded_type
{
  public:
    decoded_type(const size_t _checksum, const std::string_view _name, std::vector<member_data>&& _members, const bool _is_enum) :
      checksum(_checksum), name(_name), members(std::move(_members)), is_enum(_is_enum) {}
    void print_type_info() const;

  private:
    size_t checksum = 0;
    std::string_view name;
    std::vector<member_data> members;
    bool is_enum;
};

void decoded_type::print_type_info() const
{
  std::stringstream ss;
  ss << "Type: " << name << "\n";
  ss << "Checksum: " << checksum << "\n";

  if (is_enum)
    ss << "Values:" << "\n";
  else
    ss << "Members:" << "\n";

  for (const auto& member : members)
    ss << "  " << member.name << ": " << member.type_or_value << "\n";

  ss << "\n";

  std::cerr << ss.str();
}

std::unordered_set<std::string_view> types_being_decoded;
std::unordered_map<std::string_view, decoded_type> decoded_types;

template<typename T, bool type_is_defined = fc::reflector<T>::is_defined::value, bool type_is_enum = fc::reflector<T>::is_enum::value>
class type_decoder;

template<typename T>
class type_decoder<T,
                   fc::reflector<T>::is_enum::value ? true : false,
                   fc::reflector<T>::is_enum::value>
{
  public:    
    void decode() const {}
};

template<typename T, bool type_is_defined = fc::reflector<T>::is_defined::value, bool type_is_enum = fc::reflector<T>::is_enum::value>
class enum_decoder;

template<typename T>
class enum_decoder<T, false, fc::reflector<T>::is_enum::value>
{
  public:
    void decode() const {}
};

template<typename T>
class enum_decoder<T, true, false>
{
  public:
    void decode() const {}
};

namespace visitors
{
  struct defined_types_detector
  {
    template <typename Member, class Class, Member(Class::*member)>
    void operator()(const char *name) const
    {
      std::string_view type = fc::get_typename<Member>::name();

      if (fc::reflector<Member>::is_defined::value &&
          decoded_types.find(type) == decoded_types.end() &&
          types_being_decoded.find(type) == types_being_decoded.end())
      {
        types_being_decoded.emplace(type);

        if (fc::reflector<Member>::is_enum::value)
        {
          enum_decoder<Member> decoder;
          decoder.decode();
        }
        else
        {
          type_decoder<Member> decoder;
          decoder.decode();
        }
      }
    }
  };

  class type_checksum_calculator
  {
  public:
    type_checksum_calculator(std::vector<member_data> &_members) : members(_members) {}
    template <typename Member, class Class, Member(Class::*member)>
    void operator()(const char *name) const
    {
      checksum += pos + sizeof(Member);
      ++pos;
      std::string member_type = fc::get_typename<Member>::name();
      members.emplace_back(member_data{.type_or_value = member_type, .name = name});
    }

    size_t get_checksum() { return checksum; }

  private:
    std::vector<member_data> &members;
    mutable size_t checksum = 0;
    mutable size_t pos = 0;
  };

  class enum_checksum_calculator
  {
  public:
    enum_checksum_calculator(std::vector<member_data> &_values) : values(_values) {}

    void operator()(const char *name, const int64_t value) const
    {
      checksum += std::hash<std::string>{}(name) + value;
      values.emplace_back(member_data{.type_or_value = std::to_string(value), .name = name});
    }

    size_t get_checksum() { return checksum; }

  private:
    std::vector<member_data> &values;
    mutable size_t checksum = 0;
  };
}

template<typename T>
class enum_decoder<T, true /* is defined */, true /* is enum*/>
{
public:
  enum_decoder() : name(fc::get_typename<T>::name()) {}
  void decode() const
  {
    std::cerr << "Decoding enum: " << name << " ...\n";
    std::vector<member_data> values;
    visitors::enum_checksum_calculator checksum_calculator(values);
    fc::reflector<T>::visit(checksum_calculator);
    size_t checksum = checksum_calculator.get_checksum();
    decoded_types.try_emplace(name, decoded_type(checksum, name, std::move(values), true));
    std::cerr << "Decoding enum: " << name << " has finished.\n";
  }

private:
  std::string_view name;
};

template<typename T>
class type_decoder<T, true /* is defined */, false /* is enum*/>
{
  public:
    type_decoder() : name(fc::get_typename<T>::name()) {}
    void decode() const
    {
      std::cerr << "Decoding: " << name << " ...\n";
      detect_defined_types();
      decode_type();
      std::cerr << "Decoding: " << name << " has finished.\n";
    }

  private:
    std::string_view name;

    void detect_defined_types() const
    {
      fc::reflector<T>::visit(visitors::defined_types_detector());
    }

    void decode_type() const
    {
      std::vector<member_data> members;
      visitors::type_checksum_calculator checksum_calculator(members);
      fc::reflector<T>::visit(checksum_calculator);
      size_t checksum = checksum_calculator.get_checksum();
      decoded_types.try_emplace(name, decoded_type(checksum, name, std::move(members), false));
    }
};

template<typename T>
void add_type_to_decode()
{
  std::string_view name = fc::get_typename<T>::name();

  if (decoded_types.find(name) != decoded_types.end())
  {
    std::cerr << "Type '" << name << " already decoded, skipping ...\n";
    return;
  }

  type_decoder<T> decoder;
  decoder.decode();
}

int main( int argc, char** argv )
{
  try
  {
    auto begin = std::chrono::steady_clock::now();

    add_type_to_decode<hive::chain::account_object>();
    add_type_to_decode<hive::chain::account_metadata_object>();
    add_type_to_decode<hive::chain::account_authority_object>();
    add_type_to_decode<hive::chain::vesting_delegation_object>();
    add_type_to_decode<hive::chain::vesting_delegation_expiration_object>();
    add_type_to_decode<hive::chain::owner_authority_history_object>();
    add_type_to_decode<hive::chain::account_recovery_request_object>();
    add_type_to_decode<hive::chain::change_recovery_account_request_object>();
    add_type_to_decode<hive::chain::block_summary_object>();
    add_type_to_decode<hive::chain::comment_object>();
    add_type_to_decode<hive::chain::comment_cashout_object>();
    add_type_to_decode<hive::chain::comment_cashout_ex_object>();
    add_type_to_decode<hive::chain::comment_vote_object>();
    add_type_to_decode<hive::chain::proposal_object>();
    add_type_to_decode<hive::chain::proposal_vote_object>();
    add_type_to_decode<hive::chain::dynamic_global_property_object>();
    add_type_to_decode<hive::chain::hardfork_property_object>();
    add_type_to_decode<hive::chain::convert_request_object>();
    add_type_to_decode<hive::chain::collateralized_convert_request_object>();
    add_type_to_decode<hive::chain::escrow_object>();
    add_type_to_decode<hive::chain::savings_withdraw_object>();
    add_type_to_decode<hive::chain::liquidity_reward_balance_object>();
    add_type_to_decode<hive::chain::feed_history_object>();
    add_type_to_decode<hive::chain::limit_order_object>();
    add_type_to_decode<hive::chain::withdraw_vesting_route_object>();
    add_type_to_decode<hive::chain::decline_voting_rights_request_object>();
    add_type_to_decode<hive::chain::reward_fund_object>();
    add_type_to_decode<hive::chain::recurrent_transfer_object>();
    add_type_to_decode<hive::chain::pending_required_action_object>();
    add_type_to_decode<hive::chain::pending_optional_action_object>();
    add_type_to_decode<hive::chain::transaction_object>();
    add_type_to_decode<hive::chain::witness_vote_object>();
    add_type_to_decode<hive::chain::witness_schedule_object>();
    add_type_to_decode<hive::chain::witness_object>();
    add_type_to_decode<hive::plugins::account_by_key::key_lookup_object>();
    add_type_to_decode<hive::plugins::account_history_rocksdb::volatile_operation_object>();
    add_type_to_decode<hive::plugins::block_log_info::block_log_hash_state_object>();
    add_type_to_decode<hive::plugins::block_log_info::block_log_pending_message_object>();
    add_type_to_decode<hive::plugins::follow::follow_object>();
    add_type_to_decode<hive::plugins::follow::feed_object>();
    add_type_to_decode<hive::plugins::follow::blog_object>();
    add_type_to_decode<hive::plugins::follow::blog_author_stats_object>();
    add_type_to_decode<hive::plugins::follow::reputation_object>();
    add_type_to_decode<hive::plugins::follow::follow_count_object>();
    add_type_to_decode<hive::plugins::market_history::bucket_object>();
    add_type_to_decode<hive::plugins::market_history::order_history_object>();
    add_type_to_decode<hive::plugins::rc::rc_resource_param_object>();
    add_type_to_decode<hive::plugins::rc::rc_pool_object>();
    add_type_to_decode<hive::plugins::rc::rc_stats_object>();
    add_type_to_decode<hive::plugins::rc::rc_pending_data>();
    add_type_to_decode<hive::plugins::rc::rc_account_object>();
    add_type_to_decode<hive::plugins::rc::rc_direct_delegation_object>();
    add_type_to_decode<hive::plugins::rc::rc_usage_bucket_object>();
    add_type_to_decode<hive::plugins::reputation::reputation_object>();
    add_type_to_decode<hive::plugins::tags::tag_object>();
    add_type_to_decode<hive::plugins::tags::tag_stats_object>();
    add_type_to_decode<hive::plugins::tags::author_tag_stats_object>();
    add_type_to_decode<hive::plugins::transaction_status::transaction_status_object>();
    add_type_to_decode<hive::plugins::witness::witness_custom_op_object>();

    #ifdef HIVE_ENABLE_SMT
    add_type_to_decode<hive::chain::smt_token_object>();
    add_type_to_decode<hive::chain::account_regular_balance_object>();
    add_type_to_decode<hive::chain::account_rewards_balance_object>();
    add_type_to_decode<hive::chain::nai_pool_object>();
    add_type_to_decode<hive::chain::smt_token_emissions_object>();
    add_type_to_decode<hive::chain::smt_contribution_object>();
    add_type_to_decode<hive::chain::smt_ico_object>();
    #endif

    auto end = std::chrono::steady_clock::now();

    std::cerr << "\n----- Results: ----- \n\n";
    for (const auto& [key, val] : decoded_types)
      val.print_type_info();

    std::cerr << "\n\nDecoding time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << " microseconds.\n";
  }
  catch ( const fc::exception& e )
  {
    edump((e.to_detail_string()));
  }

  return 0;
}
