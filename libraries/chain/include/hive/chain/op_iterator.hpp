#pragma once 
#include <vector>
#include <memory>

class op_iterator
{
 public:
  using op_view_t = std::vector<char>;
  virtual ~op_iterator() = default;
  virtual bool has_next() const = 0;
  virtual op_view_t next() = 0;
};


using op_iterator_ptr = std::shared_ptr <op_iterator>;