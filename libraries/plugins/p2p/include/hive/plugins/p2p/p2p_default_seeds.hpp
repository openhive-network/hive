#pragma once

#include <vector>

namespace hive{ namespace plugins { namespace p2p {

#if defined(IS_TEST_NET) || defined(HIVE_CONVERTER_BUILD)
const std::vector< std::string > default_seeds;
#else
const std::vector< std::string > default_seeds = {
  "seed.hive.blog:2001",                 // blocktrades
  "seed.openhive.network:2001",          // gtg
  "hive-seed.roelandp.nl:2001",          // roelandp
  "hive-seed.arcange.eu:2001",           // arcange
  "anyx.io:2001",                        // anyx
  "hived.splinterlands.com:2001",        // splinterlands
  "hive-api.3speak.tv:2001",             // threespeak
  "node.mahdiyari.info:2001",            // mahdiyari
  "hive-seed.lukestokes.info:2001",      // lukestokes.mhth
  "seed.deathwing.me:2001",              // deathwing
  "hive-seed.actifit.io:2001",           // actifit
  "seed.shmoogleosukami.co.uk:2001",     // shmoogleosukami
  "hiveseed.rishipanthee.com:2001"       // rishi556
};
#endif

} } } // hive::plugins::p2p
