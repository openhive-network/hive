#pragma once 
#include <string_view>
#include <vector>
#include <memory>
#include <hive/protocol/operations.hpp>



class op_iterator
{
 public:
  using op_view_t = hive::protocol::operation;
  virtual ~op_iterator() = default;
  virtual bool has_next() const = 0;
  virtual op_view_t unpack_from_char_array_and_next() = 0;
};


using op_iterator_ptr = std::shared_ptr <op_iterator>;