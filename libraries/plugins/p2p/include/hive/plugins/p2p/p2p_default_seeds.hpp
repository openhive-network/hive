#pragma once

#include <vector>

namespace hive{ namespace plugins { namespace p2p {

#ifdef IS_TEST_NET
const std::vector< std::string > default_seeds;
#else
const std::vector< std::string > default_seeds = {
  "anyx.io:2001",                        // anyx
  "hive-seed.arcange.eu:2001",           // arcange
  "hive-seed.lukestokes.info:2001",      // lukestokes.mhth
  "hived.splinterlands.com:2001",        // aggroed
  "hiveseed-se.privex.io:2001",          // privex
  "node.mahdiyari.info:2001",            // mahdiyari
  "rpc.ausbit.dev:2001",                 // ausbitbank
  "api.hive.blog:2001",                  // blocktrades
  "seed.hivekings.com:2001",             // drakos
  "seed.liondani.com:2016",              // liondani
  "seed.openhive.network:2001",          // gtg
  "hive-seed.roelandp.nl:2001"           // roelandp
};
#endif

} } } // hive::plugins::p2p
