#pragma once

namespace hive { namespace chain {

class database;

/**
* Helper class that allows you to erase/remove from any multiindex while
* adhering to given limit. Limit can be shared between different object
* types (note that cost of removing of state object does not depend on its type).
* Negative limit (default) means there is no limit.
*/
class remove_guard
{
  public:
    explicit remove_guard( int16_t _limit = -1 ) : limit( _limit ) {}
    bool done() const { return limit == 0; }

    // removes given object (assuming we haven't reached limit yet), returns if it was successful
    template< typename ObjectType >
    bool remove( database& db, const ObjectType& obj )
    {
      bool ok = step();
      if( ok )
      {
        //note that we have to go through database::remove due to write lock;
        //alternative would be to use database::with_write_lock and pull mutable index outside, but
        //it would mean performing batch removal under continuous lock which might not be desirable
        db.remove( obj );
      }
      return ok;
    }

  private:
    bool step()
    {
      if( limit > 0 )
      {
        --limit;
        return true;
      }
      else
      {
        return !done();
      }
    }

    int16_t limit;
};

} } // hive::chain
