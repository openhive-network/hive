#include <ostream>

#include <hive/protocol/asset.hpp>
#include <hive/chain/util/tiny_asset.hpp>

namespace hive
{
namespace protocol
{
  inline std::ostream& operator<<(std::ostream& stream, const asset& obj)
  {
    return stream << obj.amount.value << ' ' << obj.symbol.to_string();
  }
}
}

namespace hive
{
namespace chain
{
  template <uint32_t _SYMBOL>
  std::ostream& operator<<(std::ostream& stream, const tiny_asset<_SYMBOL>& obj)
  {
     return stream << obj.to_asset();
  }
}
}

