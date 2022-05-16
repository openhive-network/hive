#pragma once

#include <cstdint>

namespace fc { namespace raw {

  enum class pack_mode_type : uint8_t { legacy, hf26 };

  struct pack_flags
  {
    pack_mode_type pack_mode = pack_mode_type::legacy;
  };

  class pack_manager
  {
    private:

      pack_flags flags;

    public:

      void enable_hf26_pack()
      {
        flags.pack_mode = pack_mode_type::hf26;
      }

      const pack_flags& get_pack_flags() const
      {
        return flags;
      }
  };

} }
