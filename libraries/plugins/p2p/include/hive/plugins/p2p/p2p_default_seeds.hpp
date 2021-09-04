#pragma once

#include <vector>

namespace hive{ namespace plugins { namespace p2p {

#ifdef IS_TEST_NET
const std::vector< std::string > default_seeds;
#else
const std::vector< std::string > default_seeds = {
  "api.hive.blog:2001",                  // blocktrades
  "seed.openhive.network:2001",          // gtg
  "rpc.ausbit.dev:2001",                 // ausbitbank
  "hive-seed.roelandp.nl:2001",          // roelandp
  "hive-seed.arcange.eu:2001",           // arcange
  "anyx.io:2001",                        // anyx
  "hived.splinterlands.com:2001",        // aggroed
  "node.mahdiyari.info:2001",            // mahdiyari
  "hive-seed.lukestokes.info:2001",      // lukestokes.mhth
  "seed.liondani.com:2016",              // liondani
  "hiveseed-se.privex.io:2001"           // privex
};
#endif

} } } // hive::plugins::p2p
