#pragma once

#include <vector>

namespace hive{ namespace plugins { namespace p2p {

#if defined(IS_TEST_NET) || defined(HIVE_CONVERTER_BUILD)
const std::vector< std::string > default_seeds;
#else
const std::vector< std::string > default_seeds = {
  "api.hive.blog:2001",                  // blocktrades
  "seed.openhive.network:2001",          // gtg
  "seed.ecency.com:2001",                // good-karma
  "rpc.ausbit.dev:2001",                 // ausbitbank
  "hive-seed.roelandp.nl:2001",          // roelandp
  "hive-seed.arcange.eu:2001",           // arcange
  "anyx.io:2001",                        // anyx
  "hived.splinterlands.com:2001",        // aggroed
  "seed.hive.blue:2001",                 // guiltyparties
  "hive-api.3speak.tv:2001",             // threespeak
  "node.mahdiyari.info:2001",            // mahdiyari
  "hive-seed.lukestokes.info:2001",      // lukestokes.mhth
  "api.deathwing.me:2001",               // deathwing
  "seed.liondani.com:2016",              // liondani
  "hiveseed-se.privex.io:2001",          // privex
  "seed.mintrawa.com:2001"               // mintrawa
};
#endif

} } } // hive::plugins::p2p
