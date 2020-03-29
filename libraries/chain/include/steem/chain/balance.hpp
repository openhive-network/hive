#pragma once

#include <steem/protocol/asset.hpp>

namespace steem { namespace chain {

class account_object;
class convert_request_object;
class escrow_object;
class limit_order_object;
class savings_withdraw_object;

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

      // Forces empty balance
      void check_empty() const { FC_ASSERT( WrappedAsset.amount == 0 ); }

      // Moves value from given source balance to current one
      void transfer_from( ABalance* source, int64_t value )
      {
         FC_ASSERT( WrappedAsset.symbol == source->WrappedAsset.symbol, "Can't change asset type when transfering balance." );
         WrappedAsset.amount += value;
         source->WrappedAsset.amount -= value;
         FC_ASSERT( WrappedAsset.amount >= 0 && source->WrappedAsset.amount >= 0, "Insufficient funds to complete transfer." );
      }
      // Moves whole given balance to current one
      void transfer_from( ABalance* source ) { transfer_from( source, source->WrappedAsset.amount.value ); }
      // Moves value from current balance to given one
      void transfer_to( ABalance* destination, int64_t value ) { destination->transfer_from( this, -value ); }
      // Moves whole current balance to given one
      void transfer_to( ABalance* destination ) { destination->transfer_from( this, -destination->WrappedAsset.amount.value ); }

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
      TTempBalance( protocol::asset_symbol_type id = STEEM_SYMBOL ) : ABalance( Asset ), Asset( 0, id ) {}
      ~TTempBalance() { check_empty(); }

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

   private:
      // copying of balance is forbidden, it would create tokens out of thin air
      TTempBalance( const TTempBalance& ) = delete;
      TTempBalance& operator= ( const TTempBalance& ) = delete;

      protocol::asset Asset;
};

/**
 * Wrapper for asset that is holding balance in some serializable object.
 * Use BALANCE macro to declare such object.
 */
class TBalance final : public ABalance
{
   public:
      TBalance( TBalance&& b ) : ABalance( std::move( b ) ) {}
      TBalance& operator= ( TBalance&& b ) { move_balance( std::move( b ) ); return *this; }

   private:
      TBalance( protocol::asset& balance_variable ) : ABalance( balance_variable ) {}

      // since this is just a wrapper allowing copying would lead to confusion
      TBalance( const TBalance& ) = delete;
      TBalance& operator= ( const TBalance& ) = delete;

      //list all objects that have members that hold balance - other places should use TTempBalance
      friend class account_object;
      friend class convert_request_object;
      friend class escrow_object;
      friend class limit_order_object;
      friend class savings_withdraw_object;
};

#define BALANCE( member_name, asset_symbol, getter_name )      \
   public:                                                     \
      const asset& getter_name() const { return member_name; } \
      asset& getter_name() { return member_name; }             \
   private:                                                    \
      asset member_name = asset( 0, asset_symbol )
   

} } // namespace steem::chain
