#pragma once

#include <hive/protocol/operations.hpp>
#include <hive/protocol/transaction.hpp>

#include <fc/string.hpp>

namespace hive
{
  namespace app
  {
    namespace Inserters
    {
      template <typename ContainerType>
      struct base_emplacer
      {
      };

      template <typename ContainerType>
      struct Emplacer : base_emplacer<ContainerType>
      {
        Emplacer(ContainerType &con, const typename ContainerType::value_type &val)
        {
          con.emplace(val);
        }
      };

      template <typename ContainerType>
      struct BackPusher : base_emplacer<ContainerType>
      {
        BackPusher(ContainerType &con, const typename ContainerType::value_type &val)
        {
          con.push_back(val);
        }
      };

      template <typename ContainerType>
      struct Inserter : base_emplacer<ContainerType>
      {
        Inserter(ContainerType &con, const typename ContainerType::value_type &val)
        {
          con.insert(val);
        }
      };

      template <typename ContainerType>
      struct EndInserter : base_emplacer<ContainerType>
      {
        EndInserter(ContainerType &con, const typename ContainerType::value_type &val)
        {
          con.insert(con.end(), val);
        }
      };

      template <typename ContainerType>
      struct BraceOperatorInserter : base_emplacer<ContainerType>
      {
        BraceOperatorInserter(ContainerType &con, const typename ContainerType::value_type &val)
        {
          con[val.first] = val.second;
        }
      };

      enum class Member : uint8_t
      {
        ACCOUNT_CREATE_OPERATION = 0,
        FEE = 1,
        DELEGATION = 2,
        MAX_ACCEPTED_PAYOUT = 3,
        AMOUNT = 4,
        HBD_AMOUNT = 5,
        HIVE_AMOUNT = 6,
        VESTING_SHARES = 7,
        AMOUNT_TO_SELL = 8,
        MIN_TO_RECEIVE = 9,
        REWARD_HIVE = 10,
        REWARD_HBD = 11,
        REWARD_VESTS = 12,
        SMT_CREATION_FEE = 13,
        LEP_ABS_AMOUNT = 14,
        REP_ABS_AMOUNT = 15,
        CONTRIBUTION = 16,
        DAILY_PAY = 17,
        PAYMENT = 18,

        END
      };

      std::ostream& operator<<(std::ostream& os, const Member& m);

    }; // namespace Inserters

    using namespace hive::protocol;
    using Inserters::Member;

    template <typename ContainerType, typename Inserter>
    struct permlink_extraction_visitor
    {
      typedef void result_type;

      ContainerType &result;

      template <typename op_t>
      result_type operator()(const op_t &) {}

      // ops specialization
      void operator()(const comment_operation &op)
      {
        Inserter(result, op.permlink);
        Inserter(result, op.parent_permlink);
      }

      void operator()(const comment_options_operation &op)
      {
        Inserter(result, op.permlink);
      }

      void operator()(const delete_comment_operation &op)
      {
        Inserter(result, op.permlink);
      }

      void operator()(const vote_operation &op)
      {
        Inserter(result, op.permlink);
      }

      void operator()(const create_proposal_operation &op)
      {
        Inserter(result, op.permlink);
      }

      void operator()(const update_proposal_operation &op)
      {
        Inserter(result, op.permlink);
      }
    };

    template <typename ContainerType, typename Inserter>
    struct asset_extraction_visitor
    {
      typedef void result_type;

      ContainerType &result;

      template <typename op_t>
      result_type operator()(const op_t &) {}

      // ops specialization
      void operator()(const account_create_operation &op)
      {
        put(Member::FEE, op.fee);
      }

      void operator()(const account_create_with_delegation_operation &op)
      {
        put(Member::FEE, op.fee);
        put(Member::FEE, op.delegation);
      }

      void operator()(const comment_options_operation &op)
      {
        put(Member::FEE, op.max_accepted_payout);
      }

      void operator()(const claim_account_operation &op)
      {
        put(Member::FEE, op.fee);
      }

      void operator()(const transfer_operation &op)
      {
        put(Member::AMOUNT, op.amount);
      }

      void operator()(const escrow_transfer_operation &op)
      {
        put(Member::HBD_AMOUNT, op.hbd_amount);
        put(Member::HIVE_AMOUNT, op.hive_amount);
        put(Member::FEE, op.fee);
      }

      void operator()(const escrow_release_operation &op)
      {
        put(Member::HBD_AMOUNT, op.hbd_amount);
        put(Member::HIVE_AMOUNT, op.hive_amount);
      }

      void operator()(const transfer_to_vesting_operation &op)
      {
        put(Member::AMOUNT, op.amount);
      }

      void operator()(const withdraw_vesting_operation &op)
      {
        put(Member::VESTING_SHARES, op.vesting_shares);
      }

      void operator()(const witness_update_operation &op)
      {
        put(Member::FEE, op.fee);
      }

      void operator()(const convert_operation &op)
      {
        put(Member::AMOUNT, op.amount);
      }

      void operator()(const limit_order_create_operation &op)
      {
        put(Member::AMOUNT_TO_SELL, op.amount_to_sell);
        put(Member::MIN_TO_RECEIVE, op.min_to_receive);
      }

      void operator()(const limit_order_create2_operation &op)
      {
        put(Member::AMOUNT_TO_SELL, op.amount_to_sell);
      }

      void operator()(const transfer_to_savings_operation &op)
      {
        put(Member::AMOUNT, op.amount);
      }

      void operator()(const transfer_from_savings_operation &op)
      {
        put(Member::AMOUNT, op.amount);
      }

      void operator()(const claim_reward_balance_operation &op)
      {
        put(Member::REWARD_HIVE, op.reward_hive);
        put(Member::REWARD_HBD, op.reward_hbd);
        put(Member::REWARD_VESTS, op.reward_vests);
      }

      void operator()(const delegate_vesting_shares_operation &op)
      {
        put(Member::VESTING_SHARES, op.vesting_shares);
      }

#ifdef HIVE_ENABLE_SMT

      void operator()(const smt_create_operation &op)
      {
        put(Member::SMT_CREATION_FEE, op.smt_creation_fee);
      }

      void operator()(const smt_setup_emissions_operation &op)
      {
        put(Member::LEP_ABS_AMOUNT, op.lep_abs_amount);
        put(Member::REP_ABS_AMOUNT, op.rep_abs_amount);
      }

      void operator()(const smt_contribute_operation &op)
      {
        put(Member::CONTRIBUTION, op.contribution);
      }

#endif // ifdef HIVE_ENABLE_SMT

      void operator()(const create_proposal_operation &op)
      {
        put(Member::DAILY_PAY, op.daily_pay);
      }

      void operator()(const update_proposal_operation &op)
      {
        put(Member::DAILY_PAY, op.daily_pay);
      }

      void operator()(const proposal_pay_operation &op)
      {
        put(Member::PAYMENT, op.payment);
      }

    private:

      void put(const Member m, const asset &a)
      {
        Inserter _{ result, typename ContainerType::value_type(m, a) };
      }
    };

    template <typename ContainerType = std::vector<fc::string>, typename Inserter = Inserters::BackPusher<ContainerType>>
    void operation_get_permlinks(const operation &op, ContainerType &result)
    {
      permlink_extraction_visitor<ContainerType, Inserter> vtor{result};
      op.visit(vtor);
    }

    template <typename ContainerType = std::vector<fc::string>, typename Inserter = Inserters::BackPusher<ContainerType>>
    void transaction_get_permlinks(const transaction &tx, ContainerType &result)
    {
      for (const auto &op : tx.operations)
        operation_get_permlinks(op, result);
    }

    template <typename ContainerType = std::map<Member, asset>, typename Inserter = Inserters::BraceOperatorInserter<ContainerType>>
    void operation_get_assets(const operation &op, ContainerType &result)
    {
      asset_extraction_visitor<ContainerType, Inserter> vtor{result};
      op.visit(vtor);
    }

    template <typename ContainerType = std::map<Member, asset>, typename Inserter = Inserters::BraceOperatorInserter<ContainerType>>
    void transaction_get_assets(const transaction &tx, ContainerType &result)
    {
      for (const auto &op : tx.operations)
        operation_get_assets(op, result);
    }

  } // namespace app
} // namespace hive
