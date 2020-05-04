#pragma once

#ifdef HIVE_ENABLE_SMT

#include <fc/optional.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/smt_objects/smt_token_object.hpp>
#include <hive/protocol/asset_symbol.hpp>

namespace hive { namespace chain { namespace util { namespace smt {

const smt_token_object* find_token( const database& db, uint32_t nai );
const smt_token_object* find_token( const database& db, asset_symbol_type symbol, bool precision_agnostic = false );
const smt_token_emissions_object* last_emission( const database& db, const asset_symbol_type& symbol );
fc::optional< time_point_sec > last_emission_time( const database& db, const asset_symbol_type& symbol );

} } } } // hive::chain::util::smt

#endif
