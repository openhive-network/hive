#pragma once

#include <hive/chain/hive_fwd.hpp>
#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/util/delayed_voting_processor.hpp>

namespace hive { namespace chain {

  using chainbase::t_vector;
  using t_delayed_votes = t_vector< delayed_votes_data >;

  class delayed_votes_object : public object< delayed_votes_object_type, delayed_votes_object, std::true_type /* dynamic alloc */, std::true_type /* enable no undo remove */ >
  {
    CHAINBASE_OBJECT( delayed_votes_object );
    public:
      template< typename Allocator >
      delayed_votes_object( allocator< Allocator > a, uint64_t _id )
        : id( _id ), delayed_votes( a )
      {}

      // Check if there are delayed votes
      bool has_delayed_votes() const { return !delayed_votes.empty(); }

      // Start time of oldest delayed vote bucket (the one closest to activation)
      time_point_sec get_oldest_delayed_vote_time() const
      {
        if( has_delayed_votes() )
          return ( delayed_votes.begin() )->time;
        else
          return time_point_sec::maximum();
      }

      // Sum of delayed votes
      ushare_type get_sum_delayed_votes() const { return sum_delayed_votes; }
      ushare_type& get_sum_delayed_votes() { return sum_delayed_votes; }
      void set_sum_delayed_votes( const ushare_type& value ) { sum_delayed_votes = value; }

      // Access to delayed votes collection
      t_delayed_votes& get_delayed_votes() { return delayed_votes; }
      const t_delayed_votes& get_delayed_votes() const { return delayed_votes; }

      size_t get_dynamic_alloc() const
      {
        size_t size = 0;
        size += delayed_votes.capacity() * sizeof( decltype( delayed_votes )::value_type );
        return size;
      }

    private:
      ushare_type       sum_delayed_votes = 0;    ///< sum of delayed_votes (should be changed to VEST_asset)
      t_delayed_votes   delayed_votes;

    public:
      // Helper methods for cloning data
      template<typename COLLECTION_TYPE>
      void clone( const COLLECTION_TYPE& src )
      {
        delayed_votes.clear();
        if( src.size() )
        {
          delayed_votes.reserve( src.size() );
          for( const auto& item : src )
            delayed_votes.push_back( item );
        }
      }

      template<typename COLLECTION_TYPE>
      static std::vector< delayed_votes_data > convert( const COLLECTION_TYPE& src )
      {
        std::vector< delayed_votes_data > _result;
        _result.clear();
        if( src.size() )
        {
          _result.reserve( src.size() );
          for( const auto& item : src )
            _result.push_back( item );
        }
        return _result;
      }

    CHAINBASE_UNPACK_CONSTRUCTOR(delayed_votes_object, (delayed_votes));
  };

  typedef multi_index_container<
    delayed_votes_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< delayed_votes_object, delayed_votes_object::id_type, &delayed_votes_object::get_id > >
    >,
    multi_index_allocator< delayed_votes_object >
  > delayed_votes_index;

} } // hive::chain

FC_REFLECT( hive::chain::delayed_votes_object,
          (id)(sum_delayed_votes)(delayed_votes)
        )

CHAINBASE_SET_INDEX_TYPE( hive::chain::delayed_votes_object, hive::chain::delayed_votes_index )
