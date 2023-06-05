#pragma once 
#include <vector>
#include <memory>

class op_iterator
{
 public:
  virtual ~op_iterator() = default;
  virtual bool has_next() const = 0;
  virtual std::vector<char> next() = 0;
};


using op_iterator_ptr = std::shared_ptr <op_iterator>;