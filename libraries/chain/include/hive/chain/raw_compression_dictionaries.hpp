#include <map>
#include <cstdint>
namespace hive { namespace chain {
  struct raw_dictionary_info
  {
    const void* buffer;
    unsigned size;
  };

  extern const std::map<uint8_t, raw_dictionary_info> raw_dictionaries;
} }
