#pragma once

#include <steem/protocol/asset.hpp>

namespace steem { namespace chain {

class account_object;
class convert_request_object;
class dynamic_global_property_object;
class escrow_object;
class limit_order_object;
class reward_fund_object;
class savings_withdraw_object;
class smt_contribution_object;

/**
 * Balance object keeps existing tokens and only allows them to be moved to another balance.
 * For creation and destruction of tokens see TTempBalance. For wrapping serializable asset as
 * part of objects see TBalance and BALANCE macro.
 * Use of balance objects over raw asset protects against bugs that would result in unintentional
 * change of global amount of tokens in existence (and failure of database::validate_invariants()).
 */
class ABalance
{
   public:
      const protocol::asset& as_asset() const { return WrappedAsset; }
      operator const protocol::asset& () const { return as_asset(); }
      int64_t get_value() const { return WrappedAsset.amount.value; }

      // Forces empty balance
      void check_empty() const { FC_ASSERT( WrappedAsset.amount == 0 ); }
      // Tells if balance is valid (non-negative)
      bool is_valid() const { return WrappedAsset.amount >= 0; }

      // Moves value from given source balance to current one
      void transfer_from( ABalance* source, int64_t value )
      {
         FC_ASSERT( WrappedAsset.symbol == source->WrappedAsset.symbol, "Can't change asset type when transfering balance." );
         WrappedAsset.amount += value;
         source->WrappedAsset.amount -= value;
         FC_ASSERT( is_valid() && source->is_valid(), "Insufficient funds to complete transfer." );
      }
      // Moves whole given balance to current one
      void transfer_from( ABalance* source ) { transfer_from( source, source->get_value() ); }
      // Moves value from current balance to given one
      void transfer_to( ABalance* destination, int64_t value ) { destination->transfer_from( this, value ); }
      // Moves whole current balance to given one
      void transfer_to( ABalance* destination ) { destination->transfer_from( this, get_value() ); }

   protected:
      ABalance( protocol::asset& wrapped_asset ) : WrappedAsset( wrapped_asset ) {}
      ABalance( ABalance&& b ) : WrappedAsset( b.WrappedAsset ) {}
      ABalance& operator= ( ABalance&& b ) { return move_balance( std::move( b ) ); }
      ABalance& move_balance( ABalance&& b )
      {
         if( this != &b )
         {
            check_empty();
            WrappedAsset = b.WrappedAsset;
            b.WrappedAsset.amount = 0;
         }
         return *this;
      }

   private:
      ABalance( const ABalance& ) = delete;
      ABalance& operator= ( const ABalance& ) = delete;

      protocol::asset& WrappedAsset;
};

/**
 * Temporary representation of balance with local asset holder.
 * Allows creation of new tokens or destruction of existing ones with explicit actions.
 * Must be empty when no longer used.
 */
class TTempBalance final : public ABalance
{
   public:
      TTempBalance( protocol::asset_symbol_type id ) : ABalance( Asset ), Asset( 0, id ) {}
      TTempBalance( TTempBalance&& source ) : TTempBalance( source.Asset.symbol ) { move_balance( std::move( source ) ); }
      TTempBalance& operator= ( TTempBalance&& source ) { move_balance( std::move( source ) ); return *this; }
      ~TTempBalance()
      {
         if( !std::uncaught_exception() ) //don't check - this is follow-up error caused by unwinding already in progress
           check_empty();
      }

   private:
      // copying of balance is forbidden, it would create tokens out of thin air
      TTempBalance( const TTempBalance& ) = delete;
      TTempBalance& operator= ( const TTempBalance& ) = delete;

      /**
       * Allows creation of new tokens into current balance.
       * Remember to initialize asset type in constructor according to the one you want to issue.
       * Only for inflation and conversion - in both cases adequate global counters need to be adjusted.
       */
      void issue_asset( const protocol::asset& a )
      {
         FC_ASSERT( a.amount >= 0 );
         Asset += a;
      }
      /**
       * Allows destruction of tokens from current balance.
       * Only for conversion - adequate global counter needs to be adjusted.
       */
      void burn_asset( const protocol::asset& a )
      {
         FC_ASSERT( a.amount >= 0 && Asset.amount >= a.amount );
         Asset -= a;
      }

      protocol::asset Asset;

      friend class dynamic_global_property_object; //only that class can issue or burn tokens as it contains total supply counters
};

/**
 * Wrapper for asset that is holding balance in some serializable object.
 * Use BALANCE macro to declare such object.
 */
class TBalance final : public ABalance
{
   public:
      TBalance( TBalance&& b ) : ABalance( std::move( b ) ) {}
      TBalance& operator= ( ABalance&& b ) { move_balance( std::move( b ) ); return *this; }

   private:
      TBalance( protocol::asset& balance_variable ) : ABalance( balance_variable ) {}

      // since this is just a wrapper allowing copying would lead to confusion
      TBalance( const TBalance& ) = delete;
      TBalance& operator= ( const TBalance& ) = delete;

      //list all objects that have members that hold balance - other places should use TTempBalance
      friend class account_object;
      friend class convert_request_object;
      friend class dynamic_global_property_object;
      friend class escrow_object;
      friend class limit_order_object;
      friend class reward_fund_object;
      friend class savings_withdraw_object;
      friend class smt_contribution_object;
};

#define BALANCE( member_name, getter_name )                    \
   public:                                                     \
      const asset& getter_name() const { return member_name; } \
      TBalance getter_name() { return member_name; }           \
   private:                                                    \
      asset member_name
#define HIVE_BALANCE( member_name, getter_name ) BALANCE( member_name, getter_name ) = asset( 0, STEEM_SYMBOL )
#define HBD_BALANCE( member_name, getter_name ) BALANCE( member_name, getter_name ) = asset( 0, SBD_SYMBOL )
#define VEST_BALANCE( member_name, getter_name ) BALANCE( member_name, getter_name ) = asset( 0, VESTS_SYMBOL )

} } // namespace steem::chain
