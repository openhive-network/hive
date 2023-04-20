#pragma once

#include "hive/chain/database.hpp"


namespace consensus_state_provider
{
    class cache
    {
    public:
        bool has_context(const char* context) const;
        void add(const char* context, hive::chain::database& a_db);
        void remove(const char* context);
        hive::chain::database& get_db(const char* context) const;

    };
}
