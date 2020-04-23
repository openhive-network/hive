#pragma once

#include <steem/chain/steem_object_types.hpp>

namespace steem
{
	namespace chain
	{
		template<uint32_t _SYMBOL>
		class greedy_asset
		{
		public:

			share_type value;

			greedy_asset( const asset& val )		{ set(val); }
			greedy_asset( asset&& val )				{ set( val ); }
			asset operator=( const asset& val )		{ set(val); return *this; }
			asset operator=( asset&& val )			{ set(val); return *this; }

			asset operator+=(const asset& val )		{ check(val); value += val.amount; return *this; }

			operator asset() const 					{ return asset( value, asset_symbol_type::from_asset_num( _SYMBOL) ); }

		private:

			void set(const asset& val)				{ check(val); value = val.amount; }
			void check(const asset& val) const		{ FC_ASSERT(val.symbol.asset_num == _SYMBOL); }
		};
	}
}