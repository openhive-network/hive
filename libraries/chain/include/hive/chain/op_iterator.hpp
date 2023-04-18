#pragma once
#include <hive/protocol/operations.hpp>
#include <memory>

/**
 * @file
 *
 * @brief This header enables direct iteration over operations 
 *        e,g, taken from a Postgres database.
 *
 */


namespace hive { namespace chain {

class op_iterator
{
 public:
  using op_view_t = hive::protocol::operation;
  virtual ~op_iterator() = default;
  virtual bool has_next() const = 0;
  virtual op_view_t unpack_from_char_array_and_next() = 0;
};

using op_iterator_ptr = std::unique_ptr<op_iterator>;

}}  // namespace hive::chain
