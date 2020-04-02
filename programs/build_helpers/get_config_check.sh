#!/bin/bash
diff -u \
   <(cat libraries/protocol/get_config.cpp | grep 'result[[]".*"' | cut -d '"' -f 2 | uniq | sed 's/HIVE_/STEEM_/g' | sed 's/HBD_/SBD_/g' | 	sed 's/_HBD/_SBD/g'| sort) \
   <(cat libraries/protocol/include/steem/protocol/config.hpp | grep '[#]define\s\+[A-Z0-9_]\+' | cut -d ' ' -f 2 | sort | uniq)
exit $?


