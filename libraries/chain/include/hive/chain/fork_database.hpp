#pragma once
#include <hive/protocol/block.hpp>
#include <hive/chain/full_block.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>

#include <chainbase/chainbase.hpp>

namespace hive { namespace chain {

  using hive::protocol::signed_block;
  using hive::protocol::block_id_type;

  struct fork_item
  {
    public:
      weak_ptr<fork_item>   prev;
      /**
        * Used to flag a block as invalid and prevent other blocks from
        * building on top of it.
        */
      bool                  invalid = false;
      std::shared_ptr<full_block_type> full_block;

    private:
      fork_item(){}
    public:
      fork_item(const std::shared_ptr<full_block_type>& full_block) : full_block(full_block) {}

      const signed_block& get_block() { return full_block->get_block(); }
      const signed_block_header& get_block_header() { return full_block->get_block_header(); }
      const block_id_type& previous_id() { return get_block_header().previous; }
      const block_id_type& get_block_id() { return full_block->get_block_id(); }
      uint32_t get_block_num() { return full_block->get_block_num(); }
  };
  typedef shared_ptr<fork_item> item_ptr;

  struct forkdb_lock_exception : public std::exception
  {
    explicit forkdb_lock_exception() {}
    virtual ~forkdb_lock_exception() {}

    virtual const char* what() const noexcept { return "Unable to acquire forkdb lock"; }
  };

  /**
    *  As long as blocks are pushed in order the fork
    *  database will maintain a linked tree of all blocks
    *  that branch from the start_block.  The tree will
    *  have a maximum depth of 1024 blocks after which
    *  the database will start lopping off forks.
    *
    *  Every time a block is pushed into the fork DB the
    *  block with the highest block_num will be returned.
    */
  class fork_database
  {
    public:
      typedef vector<item_ptr> branch_type;
      /// The maximum number of blocks that may be skipped in an out-of-order push
      const static int MAX_BLOCK_REORDERING = 1024;

      fork_database();
      void reset();

      void                             start_block(const std::shared_ptr<full_block_type>& full_block);
      void                             remove(block_id_type b);
      void                             set_head(shared_ptr<fork_item> h);
      bool                             is_known_block(const block_id_type& id)const;
      shared_ptr<fork_item>            fetch_block_unlocked(const block_id_type& id)const;
      shared_ptr<fork_item>            fetch_block(const block_id_type& id)const;
      vector<item_ptr>                 fetch_block_by_number_unlocked(uint32_t num) const;
      vector<item_ptr>                 fetch_block_by_number(uint32_t n)const;

      // These functions are similar to the corresponding versions in `database`,
      // except the database versions work off the `dynamic_global_properties_object`
      // (thus requiring a chainbase read lock), whereas these work off the fork
      // database's head block, only needing a fork_database read lock.
      // As such, there may be times while the chain is applying a block that these
      // functions disagree about what the head block is, so it's not appropriate
      // to rely on these values when, e.g, evaluating blocks or transactions.
      // They're good enough for the p2p layer, though.
      time_point_sec   head_block_time(fc::microseconds wait_for_microseconds = fc::microseconds())const;
      uint32_t         head_block_num(fc::microseconds wait_for_microseconds = fc::microseconds())const;
      block_id_type    head_block_id(fc::microseconds wait_for_microseconds = fc::microseconds())const;

      /**
        *  @return the new head block ( the longest fork )
        */
      shared_ptr<fork_item>            push_block(const std::shared_ptr<full_block_type>& full_block);
      shared_ptr<fork_item>            head()const;
      shared_ptr<fork_item>            head_unlocked()const;
      uint32_t                         get_oldest_block_num_unlocked()const;
      void                             pop_block();

      /**
        *  Given two head blocks, return two branches of the fork graph that
        *  end with a common ancestor (same prior block)
        */
      pair< branch_type, branch_type >  fetch_branch_from(block_id_type first, block_id_type second)const;
      shared_ptr<fork_item>            walk_main_branch_to_num_unlocked( uint32_t block_num )const;
      shared_ptr<fork_item>            walk_main_branch_to_num( uint32_t block_num )const;
      shared_ptr<fork_item>            fetch_block_on_main_branch_by_number( uint32_t block_num, fc::microseconds wait_for_microseconds = fc::microseconds() )const;
      shared_ptr<fork_item>            fetch_block_on_main_branch_by_number_unlocked( uint32_t block_num )const;
      vector<fork_item>                fetch_block_range_on_main_branch_by_number( const uint32_t first_block_num, const uint32_t count, fc::microseconds wait_for_microseconds = fc::microseconds() )const;
      std::vector<block_id_type> get_blockchain_synopsis(block_id_type reference_point, uint32_t number_of_blocks_after_reference_point, /* out */ fc::optional<uint32_t>& blocks_number_needed_from_block_log);

      struct block_id;
      struct block_num;
      struct by_previous;
      typedef boost::multi_index_container<
        item_ptr,
        boost::multi_index::indexed_by<
          boost::multi_index::hashed_unique<boost::multi_index::tag<block_id>, boost::multi_index::mem_fun<fork_item, const block_id_type&, &fork_item::get_block_id>, std::hash<fc::ripemd160>>,
          boost::multi_index::hashed_non_unique<boost::multi_index::tag<by_previous>, boost::multi_index::mem_fun<fork_item, const block_id_type&, &fork_item::previous_id>, std::hash<fc::ripemd160>>,
          boost::multi_index::ordered_non_unique<boost::multi_index::tag<block_num>, boost::multi_index::mem_fun<fork_item, uint32_t, &fork_item::get_block_num>>
        >
      > fork_multi_index_type;

      void set_max_size( uint32_t s );

#define DEBUG_FORKDB_LOCK_TIMES
      class int_incrementer2
      {
        public:
          int_incrementer2( std::atomic<int32_t>& target) : _target(target)
          {
            ++_target;
            _start_locking = fc::time_point::now();
          }
          int_incrementer2( int_incrementer2& ii) : _target(ii._target), _start_locking(ii._start_locking) { ++_target; }
          ~int_incrementer2()
          {
            --_target;
            fc::microseconds fork_lock_duration = fc::time_point::now() - _start_locking;
            fc_wlog(fc::logger::get("chainlock"), "Took ${held}µs to get and release fork_lock", ("held", fork_lock_duration.count()));
          }
          int32_t get()const { return _target; }

        private:
          std::atomic<int32_t>& _target;
          fc::time_point _start_locking;
      };

      template< typename Lambda >
      auto with_read_lock( Lambda&& callback, fc::microseconds wait_for_microseconds = fc::microseconds() ) const -> decltype( (*(Lambda*)nullptr)() )
      {
        chainbase::read_lock lock(_rw_lock, boost::defer_lock_t());

        fc_wlog(fc::logger::get("chainlock"), "trying to get fork_read_lock, read_lock_count=${_read_lock_count} write_lock_count=${_write_lock_count}", 
                ("_read_lock_count", _read_lock_count.load())("_write_lock_count", _write_lock_count.load()));
#ifdef DEBUG_FORKDB_LOCK_TIMES
        BOOST_ATTRIBUTE_UNUSED
        int_incrementer2 ii(_read_lock_count);
#endif
        if (wait_for_microseconds == fc::microseconds())
          lock.lock();
        else if (!lock.try_lock_for(boost::chrono::microseconds(wait_for_microseconds.count()))) // use lock with timeout
        {
          fc_wlog(fc::logger::get("chainlock"),"timedout getting fork_read_lock: read_lock_count=${_read_lock_count} write_lock_count=${_write_lock_count}",
                  ("_read_lock_count", _read_lock_count.load())("_write_lock_count", _write_lock_count.load()));
          CHAINBASE_THROW_EXCEPTION( forkdb_lock_exception() );
        }

        fc_wlog(fc::logger::get("chainlock"),"got fork_read_lock: read_lock_count=${_read_lock_count} write_lock_count=${_write_lock_count}",
                ("_read_lock_count", _read_lock_count.load())("_write_lock_count", _write_lock_count.load()));
      
        return callback();
      }

      template< typename Lambda >
      auto with_write_lock( Lambda&& callback ) -> decltype( (*(Lambda*)nullptr)() )
      {
        chainbase::write_lock lock(_rw_lock, boost::defer_lock_t());
        fc_wlog(fc::logger::get("chainlock"), "trying to get fork_write_lock: read_lock_count=${_read_lock_count} write_lock_count=${_write_lock_count}", 
                ("_read_lock_count", _read_lock_count.load())("_write_lock_count", _write_lock_count.load()));
#ifdef DEBUG_FORKDB_LOCK_TIMES
        BOOST_ATTRIBUTE_UNUSED
        int_incrementer2 ii(_write_lock_count);
#endif
        lock.lock();
        fc_wlog(fc::logger::get("chainlock"),"got fork_write_lock: read_lock_count=${_read_lock_count} write_lock_count=${_write_lock_count}",
                ("_read_lock_count", _read_lock_count.load())("_write_lock_count", _write_lock_count.load()));
        return callback();
      }

    private:
      mutable chainbase::read_write_mutex _rw_lock;
      mutable std::atomic<int32_t> _read_lock_count = {0};
      mutable std::atomic<int32_t> _write_lock_count = {0};

    private:
      /** @return a pointer to the newly pushed item */
      void _push_block(const item_ptr& b );
      void _push_next(const item_ptr& newly_inserted);

      uint32_t                 _max_size = 1024;

      fork_multi_index_type    _unlinked_index;
      fork_multi_index_type    _index;
      shared_ptr<fork_item>    _head;
  };

} } // hive::chain
