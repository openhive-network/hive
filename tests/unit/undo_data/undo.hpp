#pragma once

#include <hive/chain/database.hpp>
#include <fstream>

namespace hive { namespace chain {

  /*
    The class 'undo_scenario' simplifies writing tests from 'undo_tests' group.
    a) There are implemented wrappers for 3 methods: database::create, database::modify, database::remove.
    b) Methods 'remember_old_values' and 'check' help to compare state before and after running 'undo' mechanism.
  */
  template< typename Object >
  class undo_scenario
  {
    private:

      chain::database& db;
      std::list< Object > old_values;
      size_t initial_additional_allocation = 0;

    public:

      undo_scenario( chain::database& _db ): db( _db )
      {
      }

      virtual ~undo_scenario(){}

      template< typename Index >
      void remember_additional_allocation()
      {
        const auto& index = db.get_index<Index>();
        initial_additional_allocation = index.get_item_additional_allocation();
      }

      //Proxy method for `database::create`.
      template< typename ... CALL_ARGS >
      const Object& create( CALL_ARGS&&... call_args )
      {
        return db.create< Object >( std::forward<CALL_ARGS>( call_args )... );
      }

      //Proxy method for `database::modify`.
      template< typename CALL >
      const Object& modify( const Object& old_obj, CALL&& call )
      {
        db.modify( old_obj, std::forward<CALL>( call ) );
        return old_obj;
      }

      //Proxy method for `database::remove`.
      const void remove( const Object& old_obj )
      {
        db.remove( old_obj );
      }

      //Save old objects before launching 'undo' mechanism.
      //The objects are always sorted using 'by_id' index.
      template< typename Index >
      void remember_old_values()
      {
        old_values.clear();


        const auto& idx = db.get_index< Index, by_id >();
        auto it = idx.begin();

        while( it != idx.end() )
          old_values.emplace_back( ( it++ )->copy_chain_object() );
      }

      //Get size of given index.
      template< typename Index >
      uint32_t size()
      {
        const auto& idx = db.get_index< Index, by_id >();
        return idx.size();
      }

      //Reads current objects( for given index ) and compares with objects which have been saved before.
      //The comparision is according to 'id' field.
      template< typename Index >
      bool check()
      {
        try
        {

          const auto& idx = db.get_index< Index, by_id >();

          uint32_t idx_size = idx.size();
          uint32_t old_size = old_values.size();
          if( idx_size != old_size )
            return false;

          auto it = idx.begin();
          auto it_end = idx.end();

          auto it_old = old_values.begin();

          while( it != it_end )
          {
            const Object& actual = *it;
            const Object& old = *it_old;
            if( actual.get_id() != old.get_id() )
              return false;

            ++it;
            ++it_old;
          }
        }
        FC_LOG_AND_RETHROW()

        return true;
      }

      template< typename Index >
      bool check_additional_allocation(size_t expected_additional_allocation)
      {
        const auto& index = db.get_index<Index>();
        const size_t actual_additional_allocation = index.get_item_additional_allocation();
        if (initial_additional_allocation + expected_additional_allocation != actual_additional_allocation)
        {
          ilog("Additional allocation mismatch, remembered ${initial}, current ${current}, expected ${expected}", ("initial", initial_additional_allocation)("current", actual_additional_allocation)("expected", expected_additional_allocation));
          return false;
        }
        return true;
      }

  };

  /*
    The class 'undo_scenario' simplifies writing tests from 'undo_tests' group.
    A method 'undo_begin' allows to enable 'undo' mechanism artificially.
    A method 'undo_end' allows to revert all changes.
  */
  class undo_db
  {
    private:

      database::session* session = nullptr;
      chain::database& db;

    protected:
    public:

      undo_db( chain::database& _db ): db( _db )
      {
      }

      //Enable 'undo' mechanism.
      void undo_begin()
      {
        if( session )
        {
          delete session;
          session = nullptr;
        }

        session = new database::session( db.start_undo_session() );
      }

      //Revert all changes.
      void undo_end()
      {
        if( session )
          session->undo();
      }
  };

} }
