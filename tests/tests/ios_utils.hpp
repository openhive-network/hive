#include <ostream>
#include <string>

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

  template<typename _Storage>
  inline std::ostream& operator<<(std::ostream& stream, const fixed_string_impl<_Storage>& s)
  {
     return stream << static_cast<std::string>(s);
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

namespace fc
{
  inline std::ostream& operator<<(std::ostream& stream, const time_point_sec& t)
  {
     return stream << t.to_iso_string();
  }
}
