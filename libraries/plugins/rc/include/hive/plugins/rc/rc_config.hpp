#pragma once

#include <hive/protocol/config.hpp>

#ifndef HIVE_RC_SPACE_ID
#define HIVE_RC_SPACE_ID 16
#endif

#define HIVE_RC_DRC_FLOAT_LEVEL    (20*HIVE_1_PERCENT)
#define HIVE_RC_MAX_DRC_RATE       1000

#define HIVE_RC_REGEN_TIME         (60*60*24*5) //5 days
#define HIVE_RC_BUCKET_TIME_LENGTH (60*60) //1 hour
#define HIVE_RC_WINDOW_BUCKET_COUNT 24
// 2020.748973 VESTS == 1.000 HIVE when HF20 occurred on mainnet
// TODO: What should this value be for testnet?
#define HIVE_RC_HISTORICAL_ACCOUNT_CREATION_ADJUSTMENT      2020748973

// 1.66% is ~2 hours of regen.
// 2 / ( 24 * 5 ) = 0.01666...
#define HIVE_RC_MAX_NEGATIVE_PERCENT 166

