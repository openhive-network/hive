#pragma once

namespace fc { namespace raw {

  struct pack_flags
  {
    bool nai = false;
  };

  using fc::raw::pack_flags;

  class pack_manager
  {
    private:

      pack_flags flags;

    public:

      void enable_nai()
      {
        flags.nai = true;
      }

      const pack_flags& get_pack_flags() const
      {
        return flags;
      }
  };

} }
