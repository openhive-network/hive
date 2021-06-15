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

    }; // namespace Inserters

    using namespace hive::protocol;

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
  } // namespace app
} // namespace hive
