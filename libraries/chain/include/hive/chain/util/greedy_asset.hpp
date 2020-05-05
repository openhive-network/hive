#pragma once

#include <hive/chain/steem_object_types.hpp>

namespace hive
{
	namespace chain
	{
		template<uint32_t _SYMBOL>
		class greedy_asset
		{
		public:

			share_type amount;

			greedy_asset( const asset& val )		{ set(val); }
			greedy_asset( asset&& val )				{ set( val ); }
			asset operator=( const asset& val )		{ set(val); return *this; }
			asset operator=( asset&& val )			{ set(val); return *this; }

			asset operator+=(const asset& val )		{ check(val); amount += val.amount; return *this; }
			asset operator-=(const asset& val )		{ check(val); amount -= val.amount; return *this; }

			operator asset() const 					{ return to_asset(); }

			asset to_asset() const					{ return asset( amount, asset_symbol_type::from_asset_num( _SYMBOL) ); }
			
		private:

			void	set(const asset& val)			{ check(val); amount = val.amount; }
			void	check(const asset& val) const	{ FC_ASSERT(val.symbol.asset_num == _SYMBOL); }
		};

		using greedy_HBD_asset		= greedy_asset<	HIVE_ASSET_NUM_HBD		>;
		using greedy_STEEM_asset	= greedy_asset<	HIVE_ASSET_NUM_HIVE	>;
		using greedy_VEST_asset		= greedy_asset<	HIVE_ASSET_NUM_VESTS	>;

		template<uint32_t _SYMBOL>
		bool operator==(const greedy_asset<_SYMBOL>& obj1, const greedy_asset<_SYMBOL>& obj2 ) { return obj1.to_asset() == obj2.to_asset(); }

		template<uint32_t _SYMBOL>
		bool operator!=(const greedy_asset<_SYMBOL>& obj1, const greedy_asset<_SYMBOL>& obj2 ) { return !(obj1.to_asset() == obj2.to_asset()); }

		template<uint32_t _SYMBOL>
		asset operator-(const greedy_asset<_SYMBOL>& obj1) { return -static_cast<asset>(obj1); }

	}
}

FC_REFLECT( hive::chain::greedy_HBD_asset,   (amount) )
FC_REFLECT( hive::chain::greedy_STEEM_asset,	(amount) )
FC_REFLECT( hive::chain::greedy_VEST_asset,	(amount) )
