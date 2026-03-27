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
  "node.mahdiyari.info:2001",            // mahdiyari
  "seed.deathwing.me:2001",              // deathwing
  "hive-seed.actifit.io:2001",           // actifit
  "hiveseed.rishipanthee.com:2001"       // rishi556
};
#endif

} } } // hive::plugins::p2p
