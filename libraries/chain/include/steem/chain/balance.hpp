#pragma once

#include <steem/protocol/asset.hpp>

namespace steem { namespace chain {

class account_object;

/**
 * Balance object keeps existing tokens and only allows them to be moved to another balance.
 * For creation and destruction of tokens see TTempBalance. For keeping serializable balance as
 * part of objects see TBalance.
 * Use of balance objects over raw asset protects against bugs that would result in unintentional
 * change of global amount of tokens in existence (and failure of database::validate_invariants()).
 */
class ABalance : protected protocol::asset
{
   public:
      const asset& as_asset() const { return *this; }
      operator const asset& () const { return as_asset(); }

      // Forces empty balance
      void check_empty() const { FC_ASSERT( amount == 0 ); }

      // Moves value from given source balance to current one
      void transfer_from( ABalance* source, int64_t value )
      {
         FC_ASSERT( symbol == source->symbol, "Can't change asset type when transfering balance." );
         amount += value;
         source->amount -= value;
         FC_ASSERT( amount >= 0 && source->amount >= 0, "Insufficient funds to complete transfer." );
      }
      // Moves whole given balance to current one
      void transfer_from( ABalance* source ) { transfer_from( source, source->amount.value ); }
      // Moves value from current balance to given one
      void transfer_to( ABalance* destination, int64_t value ) { destination->transfer_from( this, -value ); }
      // Moves whole current balance to given one
      void transfer_to( ABalance* destination ) { destination->transfer_from( this, -destination->amount.value ); }

   protected:
      ABalance( protocol::asset_symbol_type id ) : asset( 0, id ) {}
      ABalance( ABalance&& b ) : asset( b ) { b.amount = 0; }
      ABalance& operator= ( ABalance&& b )
      {
         if( this != &b )
         {
            check_empty();
            amount = b.amount;
            b.amount = 0;
         }
         return *this;
      }
      
   private:
      // copying of balance is forbidden - it would create extra tokens out of thin air
      ABalance( const ABalance& ) = delete;
      ABalance& operator= ( const ABalance& ) = delete;
};

/**
 * Temporary representation of balance.
 * Allows creation of new tokens or destruction of existing ones with explicit actions.
 * Must be empty when no longer used.
 */
class TTempBalance final : public ABalance
{
   public:
      TTempBalance( protocol::asset_symbol_type id = STEEM_SYMBOL ) : ABalance( id ) {}
      TTempBalance( ABalance&& b ) : ABalance( std::move(b) ) {}
      //TTempBalance& operator= ( ABalance&& b ) { *this = std::move( b ); return *this; }
      ~TTempBalance() { check_empty(); }

      /**
       * Allows creation of new tokens into current balance.
       * Remember to initialize asset type in constructor according to the one you want to issue.
       * Only for inflation and conversion - in both cases adequate global counters need to be adjusted.
       */
      void issue_asset( const asset& a )
      {
         FC_ASSERT( a.amount >= 0 );
         *this += a;
      }
      /**
       * Allows destruction of tokens from current balance.
       * Only for conversion - adequate global counter needs to be adjusted.
       */
      void burn_asset( const asset& a )
      {
         FC_ASSERT( a.amount >= 0 && amount >= a.amount );
         *this -= a;
      }

   private:
      // copying of balance is forbidden
      TTempBalance( const TTempBalance& ) = delete;
      TTempBalance& operator= ( const TTempBalance& ) = delete;
};

class TBalance final : public ABalance
{
   public:
      TBalance( TBalance&& b ) : ABalance( std::move( b ) ) {}
      TBalance& operator= ( TBalance&& b ) { *this = std::move( b ); return *this; }

   private:
      TBalance( protocol::asset_symbol_type id = STEEM_SYMBOL ) : ABalance( id ) {}

      // copying of balance is forbidden
      TBalance( const TBalance& ) = delete;
      TBalance& operator= ( const TBalance& ) = delete;

      friend class account_object;
};

} } // namespace steem::chain

FC_REFLECT_DERIVED( steem::chain::TBalance, (steem::protocol::asset), BOOST_PP_SEQ_NIL )
