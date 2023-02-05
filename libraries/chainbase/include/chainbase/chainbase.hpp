#pragma once

#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/containers/set.hpp>
#include <boost/interprocess/containers/flat_map.hpp>
#include <boost/interprocess/containers/deque.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/sync/interprocess_sharable_mutex.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <boost/interprocess/sync/file_lock.hpp>

#include <boost/any.hpp>
#include <boost/chrono.hpp>
#include <boost/config.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>
#include <boost/throw_exception.hpp>

#include <chainbase/allocators.hpp>
#include <chainbase/state_snapshot_support.hpp>
#include <chainbase/util/object_id.hpp>

#include <fc/exception/exception.hpp>

#include <array>
#include <atomic>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <typeindex>
#include <typeinfo>

#ifndef CHAINBASE_NUM_RW_LOCKS
  #define CHAINBASE_NUM_RW_LOCKS 10
#endif

#ifdef CHAINBASE_CHECK_LOCKING
  #define CHAINBASE_REQUIRE_READ_LOCK(m, t) require_read_lock(m, typeid(t).name())
  #define CHAINBASE_REQUIRE_WRITE_LOCK(m, t) require_write_lock(m, typeid(t).name())
#else
  #define CHAINBASE_REQUIRE_READ_LOCK(m, t)
  #define CHAINBASE_REQUIRE_WRITE_LOCK(m, t)
#endif

//redirect exceptions from chainbase to the same place as the one from FC_ASSERT
#define CHAINBASE_THROW_EXCEPTION( exception )                 \
  do {                                                         \
    auto ex = exception;                                       \
    if( fc::enable_record_assert_trip )                        \
      fc::record_assert_trip( __FILE__, __LINE__, ex.what() ); \
    BOOST_THROW_EXCEPTION( ex );                               \
  } while( false )

namespace helpers
{
  struct environment_extension_resources
  {
    using t_plugins = std::set< std::string >;
    using logger_type = std::function< void( const std::string& ) >;

    const std::string&   version_info;
    const t_plugins      plugins;

    logger_type          logger;

    environment_extension_resources( const std::string& _version_info, t_plugins&& _plugins, logger_type&& _logger )
                      : version_info( _version_info ), plugins( _plugins ), logger( _logger )
    {
    }

  };

  struct index_statistic_info
  {
    std::string _value_type_name;
    size_t      _item_count = 0;
    size_t      _item_sizeof = 0;
    /// Additional (ie dynamic container) allocations held in stored items
    size_t      _item_additional_allocation = 0;
    /// Additional memory used for container internal structures (like tree nodes).
    size_t      _additional_container_allocation = 0;
  };

  template <class IndexType>
  void gather_index_static_data(const IndexType& index, index_statistic_info* info)
  {
#if BOOST_VERSION >= 107400
#define MULTIINDEX_NODE_TYPE final_node_type
#else
#define MULTIINDEX_NODE_TYPE node_type
#endif
    static_assert( sizeof( typename IndexType::MULTIINDEX_NODE_TYPE ) >= sizeof( typename IndexType::value_type ) );

    info->_value_type_name = boost::core::demangle(typeid(typename IndexType::value_type).name());
    info->_item_count = index.size();
    info->_item_sizeof = sizeof(typename IndexType::value_type);
    info->_item_additional_allocation = 0;
    size_t pureNodeSize = sizeof(typename IndexType::MULTIINDEX_NODE_TYPE) -
      sizeof(typename IndexType::value_type);
    info->_additional_container_allocation = info->_item_count*pureNodeSize;
  }

  template <class IndexType>
  class index_statistic_provider
  {
  public:
    index_statistic_info gather_statistics(const IndexType& index, bool onlyStaticInfo) const
    {
      index_statistic_info info;
      gather_index_static_data(index, &info);
      return info;
    }
  };
} /// namespace helpers

namespace chainbase {

  namespace bip = boost::interprocess;
  namespace bfs = boost::filesystem;
  using std::unique_ptr;
  using std::vector;

  enum open_flags
  {
    skip_nothing               = 0,
    skip_env_check             = 1 << 0 // Skip environment check on db open
  };

  struct strcmp_less
  {
    bool operator()( const shared_string& a, const shared_string& b )const
    {
      return less( a.c_str(), b.c_str() );
    }

#ifndef ENABLE_STD_ALLOCATOR
    bool operator()( const shared_string& a, const std::string& b )const
    {
      return less( a.c_str(), b.c_str() );
    }

    bool operator()( const std::string& a, const shared_string& b )const
    {
      return less( a.c_str(), b.c_str() );
    }
#endif

    private:
      inline bool less( const char* a, const char* b )const
      {
        return std::strcmp( a, b ) < 0;
      }
  };

  template<uint16_t TypeNumber, typename Derived>
  struct object
  {
    typedef oid<Derived> id_type;
    typedef oid_ref<Derived> id_ref_type;
    static const uint16_t type_id = TypeNumber;

  };

  /** this class is ment to be specified to enable lookup of index type by object type using
    * the SET_INDEX_TYPE macro.
    **/
  template<typename T>
  struct get_index_type {};

  /**
    *  This macro must be used at global scope and OBJECT_TYPE and INDEX_TYPE must be fully qualified
    */
  #define CHAINBASE_SET_INDEX_TYPE( OBJECT_TYPE, INDEX_TYPE )  \
  namespace chainbase { template<> struct get_index_type<OBJECT_TYPE> { typedef INDEX_TYPE type; }; }

  #define CHAINBASE_OBJECT_1( object_class ) CHAINBASE_OBJECT_false( object_class )
  #define CHAINBASE_OBJECT_2( object_class, allow_default ) CHAINBASE_OBJECT_##allow_default( object_class )
  #define CHAINBASE_OBJECT_true( object_class ) CHAINBASE_OBJECT_COMMON( object_class ); public: object_class() : id(0) {} private:
  #define CHAINBASE_OBJECT_COMMON( object_class )                     \
  private:                                                            \
    id_type id;                                                       \
    object_class( const object_class& source ) = default;             \
    /* problem with this being private? you most likely did           \
      auto chain_object = db.get(...);                                \
      instead of                                                      \
      auto& chain_object_ref = db.get(...);                           \
      In case you actually need copy, use copy_chain_object() below   \
    */                                                                \
    object_class& operator= ( const object_class& source ) = default; \
  public:                                                             \
    id_type get_id() const { return id; }                             \
    object_class( object_class&& source ) = default;                  \
    object_class& operator= ( object_class&& source ) = default;      \
    object_class copy_chain_object() const { return *this; }          \
    friend class fc::reflector< object_class >

  #define CHAINBASE_OBJECT_false( object_class ) CHAINBASE_OBJECT_COMMON( object_class ); object_class() = delete; \
  private:

  /**
    * use at the start of any class derived from chainbase::object<>, f.e.:
    * CHAINBASE_OBJECT( account_object ) or
    * CHAINBASE_OBJECT( dynamic_global_property_object, true )
    * first parameter is a class name, second (true or false, default false) tells if default constructor should be allowed
    */
  #define CHAINBASE_OBJECT( ... ) BOOST_PP_OVERLOAD(CHAINBASE_OBJECT_,__VA_ARGS__)(__VA_ARGS__)


  #define CHAINBASE_ALLOCATED_MEMBERS( r, init, member ) , member( init )
  #define CHAINBASE_DEFAULT_CONSTRUCTOR( OBJECT_TYPE, ALLOCATED_MEMBERS... )               \
  template<typename Constructor, typename Allocator>                                       \
  OBJECT_TYPE( Allocator&& a, uint64_t _id, Constructor&& c )                              \
    : id( _id ) BOOST_PP_SEQ_FOR_EACH( CHAINBASE_ALLOCATED_MEMBERS, a, ALLOCATED_MEMBERS ) \
  { c(*this); }

  #define CHAINBASE_UNPACK_CONSTRUCTOR( OBJECT_TYPE, ALLOCATED_MEMBERS... )                \
  private: template<typename Allocator>                                                    \
  OBJECT_TYPE( Allocator&& a, uint64_t _id, std::function<void(OBJECT_TYPE&)> unpackFn)    \
    : id( _id ) BOOST_PP_SEQ_FOR_EACH( CHAINBASE_ALLOCATED_MEMBERS, a, ALLOCATED_MEMBERS ) \
  { unpackFn(*this); }                                                                     \
  template <class T> friend class chainbase::generic_index


  template< typename value_type >
  class undo_state
  {
    public:
      typedef typename value_type::id_type                      id_type;
      typedef allocator< std::pair<const id_type, value_type> > id_value_allocator_type;
      typedef allocator< id_type >                              id_allocator_type;

      template<typename T>
      undo_state( allocator<T> al )
      :old_values( id_value_allocator_type( al ) ),
        removed_values( id_value_allocator_type( al ) ),
        new_ids( id_allocator_type( al ) ){}

      typedef boost::interprocess::map< id_type, value_type, std::less<id_type>, id_value_allocator_type >  id_value_type_map;
      typedef boost::interprocess::set< id_type, std::less<id_type>, id_allocator_type >                    id_type_set;

      id_value_type_map            old_values;
      id_value_type_map            removed_values;
      id_type_set                  new_ids;
      id_type                      old_next_id = id_type(0);
      int64_t                      revision = 0;
  };

  /**
    * The code we want to implement is this:
    *
    * ++target; try { ... } finally { --target }
    *
    * In C++ the only way to implement finally is to create a class
    * with a destructor, so that's what we do here.
    */
  class session_int_incrementer
  {
  public:
    session_int_incrementer(int32_t& target) :
      _target(&target)
    {
      ++*_target;
    }
    session_int_incrementer(session_int_incrementer&& rhs) :
      _target(rhs._target)
    {
      rhs._target = nullptr;
    }
    ~session_int_incrementer()
    {
      if (_target)
        --*_target;
    }

  private:
    int32_t* _target;
  };

  class int_incrementer
  {
  public:
    int_incrementer(std::atomic<int32_t>& target, const char* const lock_type, uint32_t lock_serial_number) :
      _target(target),
      _start_locking(fc::time_point::now()),
      _lock_type(lock_type),
      _lock_serial_number(lock_serial_number)
    {
      _target.fetch_add(1, std::memory_order_relaxed);
    }
    ~int_incrementer()
    {
      _target.fetch_sub(1, std::memory_order_relaxed);
      fc::microseconds lock_duration = fc::time_point::now() - _start_locking;
      fc_wlog(fc::logger::get("chainlock"), "Took ${held}µs to get and release chainbase ${_lock_type} lock (#${_lock_serial_number})", ("held", lock_duration.count())(_lock_type)(_lock_serial_number));
    }
    int32_t get() const { return _target; }

  private:
    std::atomic<int32_t>& _target;
    fc::time_point _start_locking;
    const char* const _lock_type; // will be either "read" or "write"
    uint32_t _lock_serial_number; // allows us to associate the "locking" log with the "releasing" log
  };

  /**
    *  The value_type stored in the multiindex container must have a integer field accessible through
    *  constant function 'get_id'.  This will be the primary key and it will be assigned and managed by generic_index.
    *
    *  Additionally, the constructor for value_type must take an allocator
    */
  template<typename MultiIndexType>
  class generic_index
  {
    private:
      std::string get_type_name() const
      {
        return boost::core::demangle( typeid( typename index_type::value_type ).name() );
      }

    public:
      typedef MultiIndexType                                        index_type;
      typedef typename index_type::value_type                       value_type;
      typedef typename value_type::id_type                          id_type;
      typedef allocator< generic_index >                            allocator_type;
      typedef undo_state< value_type >                              undo_state_type;

      generic_index( allocator<value_type> a, bfs::path p )
      :_stack(a),_indices( a, p ),_size_of_value_type( sizeof(typename MultiIndexType::value_type) ),_size_of_this(sizeof(*this)) {}

      generic_index( allocator<value_type> a )
      :_stack(a),_indices( a ),_size_of_value_type( sizeof(typename MultiIndexType::value_type) ),_size_of_this(sizeof(*this)) {}

      void validate()const {
        if( sizeof(typename MultiIndexType::value_type) != _size_of_value_type || sizeof(*this) != _size_of_this )
        {
          char buffer[512];
          std::snprintf( buffer, sizeof(buffer), "content of memory does not match data expected by executable. type-name: %s new-value-type-size: %s old-value-type-size: %s new-generic-index-size: %s old-generic-index-size: %s",
            get_type_name().c_str(),
            std::to_string( sizeof(typename MultiIndexType::value_type) ).c_str(),
            std::to_string( _size_of_value_type ).c_str(),
            std::to_string( sizeof(*this) ).c_str(),
            std::to_string( _size_of_this ).c_str() );

          CHAINBASE_THROW_EXCEPTION( std::runtime_error(buffer) );
        }
      }

      /**
        * Construct a new element in the multi_index_container.
        * Set the ID to the next available ID, then increment _next_id and fire off on_create().
        */
      template<typename ...Args>
      const value_type& emplace( Args&&... args ) {
        auto new_id = _next_id;

        auto insert_result = _indices.emplace( _indices.get_allocator(), new_id, std::forward<Args>( args )... );

        if( !insert_result.second ) {
          CHAINBASE_THROW_EXCEPTION(std::logic_error(
            "could not insert object, most likely a uniqueness constraint was violated inside index holding types: " + get_type_name()));
        }

        ++_next_id;
        on_create( *insert_result.first );
        return *insert_result.first;
      }

      /**
        * Construct a new element and puts in the multi_index_container, basing on snapshot stream.
        */
      void unpack_from_snapshot(typename value_type::id_type objectId, std::function<void(value_type&)>&& unpack,
        std::function<std::string(const fc::variant&)>&& preetify) {
        _next_id = objectId;
        value_type tmp(_indices.get_allocator(), objectId, std::move(unpack));

        auto insert_result = _indices.emplace(std::move(tmp));

        if(!insert_result.second) {
          std::string s = preetify(fc::variant(tmp));

          const auto& conflictingObject = *insert_result.first;
          std::string s2 = preetify(fc::variant(conflictingObject));
          std::string msg = "could not insert unpacked object, most likely a uniqueness constraint was violated: `" + s +
            std::string("' conflicting object:`") + s2 + "'";

          CHAINBASE_THROW_EXCEPTION(std::logic_error(msg));
          }

        ++_next_id;

        on_create(*insert_result.first);
        }

      template<typename Modifier>
      void modify( const value_type& obj, Modifier&& m ) {
        on_modify( obj );

        fc::exception_ptr fc_exception_ptr;
        std::exception_ptr std_exception_ptr;

        auto safe_modifier = [&m, &fc_exception_ptr, &std_exception_ptr](value_type& obj) {
          try
          {
            m(obj);
          }
          catch(const fc::exception& e)
          {
            fc_exception_ptr = e.dynamic_copy_exception();
          }
          catch(...)
          {
            std_exception_ptr = std::current_exception();
          }
        };

        auto itr = _indices.iterator_to( obj );

        auto ok = _indices.modify( itr, safe_modifier);

        if(fc_exception_ptr)
          fc_exception_ptr->dynamic_rethrow_exception();
        else if(std_exception_ptr)
          std::rethrow_exception(std_exception_ptr);

        if(!ok)
        {
          CHAINBASE_THROW_EXCEPTION(std::logic_error(
            "Could not modify object, most likely a uniqueness constraint was violated inside index holding types: " + get_type_name()));
        }
      }

      void remove( const value_type& obj ) {
        on_remove( obj );
        _indices.erase( _indices.iterator_to( obj ) );
      }

      template< typename ByIndex >
      typename MultiIndexType::template index_iterator<ByIndex>::type erase(typename MultiIndexType::template index_iterator<ByIndex>::type objI) {
        auto& idx = _indices.template get< ByIndex >();
        on_remove( *objI );
        return idx.erase(objI);
      }

      template< typename ByIndex, typename ExternalStorageProcessor, typename Iterator = typename MultiIndexType::template index_iterator<ByIndex>::type >
      void move_to_external_storage(Iterator begin, Iterator end, ExternalStorageProcessor&& processor)
      {
        auto& idx = _indices.template get< ByIndex >();

        for(auto objectI = begin; objectI != end;)
        {
          bool allow_removal = processor(*objectI);

          auto nextI = objectI;
          ++nextI;
          if(allow_removal)
          {
            auto successor = idx.erase(objectI);
            FC_ASSERT(successor == nextI);
            objectI = successor;
          }
          else
          {
            /// Move to the next object
            objectI = nextI;
          }
        }
      }

      template<typename CompatibleKey>
      const value_type* find( CompatibleKey&& key )const {
        auto itr = _indices.find( std::forward<CompatibleKey>( key ) );
        if( itr != _indices.end() ) return &*itr;
        return nullptr;
      }

      template<typename CompatibleKey>
      const value_type& get( CompatibleKey&& key )const {
        auto ptr = find( std::forward<CompatibleKey>( key ) );
        if( !ptr ) CHAINBASE_THROW_EXCEPTION( std::out_of_range("key not found") );
        return *ptr;
      }

      index_type& mutable_indices() { return _indices; }

      const index_type& indices()const { return _indices; }

      void clear() { _indices.clear(); }

      class session {
        public:
          session( session&& mv )
          :_index(mv._index),_apply(mv._apply){ mv._apply = false; }

          ~session() {
            if( _apply ) {
              _index.undo();
            }
          }

          /** leaves the UNDO state on the stack when session goes out of scope */
          void push()   { _apply = false; }
          /** combines this session with the prior session */
          void squash() { if( _apply ) _index.squash(); _apply = false; }
          void undo()   { if( _apply ) _index.undo();  _apply = false; }

          session& operator = ( session&& mv ) {
            if( this == &mv ) return *this;
            if( _apply ) _index.undo();
            _apply = mv._apply;
            mv._apply = false;
            return *this;
          }

          int64_t revision()const { return _revision; }

        private:
          friend class generic_index;

          session( generic_index& idx, int64_t revision )
          :_index(idx),_revision(revision) {
            if( revision == -1 )
              _apply = false;
          }

          generic_index& _index;
          bool           _apply = true;
          int64_t        _revision = 0;
      };

      // TODO: This function needs some work to make it consistent on failure.
      session start_undo_session()
      {
        ++_revision;

        _stack.emplace_back( _indices.get_allocator() );
        _stack.back().old_next_id = _next_id;
        _stack.back().revision = _revision;
        return session( *this, _revision );
      }

      const index_type& indicies()const { return _indices; }
      int64_t revision()const { return _revision; }

      id_type get_next_id() const
      {
        return _next_id;
      }

      void store_next_id(id_type next_id)
      {
        _next_id = next_id;
      }

      /**
        *  Restores the state to how it was prior to the current session discarding all changes
        *  made between the last revision and the current revision.
        */
      void undo() {
        if( !enabled() ) return;

        auto& head = _stack.back();

        for( auto& item : head.old_values ) {
          bool ok = false;
          auto itr = _indices.find( item.second.get_id() );
          if( itr != _indices.end() )
          {
            ok = _indices.modify( itr, [&]( value_type& v ) {
              v = std::move( item.second );
            });
          }
          else
          {
            ok = _indices.emplace( std::move( item.second ) ).second;
          }

          if( !ok )
          {
            CHAINBASE_THROW_EXCEPTION(std::logic_error(
              "Could not modify object, most likely a uniqueness constraint was violated inside index holding types: "
                + get_type_name()));
          }
        }

        for( const auto& id : head.new_ids )
        {
          auto position = _indices.find(id);

          if(position == _indices.end())
          {
            CHAINBASE_THROW_EXCEPTION(std::logic_error("unable to find object with id: " +
              std::to_string(id) + "in the index holding types: " + get_type_name()));
          }

            _indices.erase( position );
        }
        _next_id = head.old_next_id;

        for( auto& item : head.removed_values ) {
          bool ok = _indices.emplace( std::move( item.second ) ).second;
          if( !ok )
          {
            CHAINBASE_THROW_EXCEPTION(std::logic_error(
              "Could not restore object, most likely a uniqueness constraint was violated inside index holding types: " + get_type_name()));
          }
        }

        _stack.pop_back();
        --_revision;
      }

      /**
        *  This method works similar to git squash, it merges the change set from the two most
        *  recent revision numbers into one revision number (reducing the head revision number)
        *
        *  This method does not change the state of the index, only the state of the undo buffer.
        */
      void squash()
      {
        if( !enabled() ) return;
        if( _stack.size() == 1 ) {
          _stack.pop_front();
          return;
        }

        auto& state = _stack.back();
        auto& prev_state = _stack[_stack.size()-2];

        // An object's relationship to a state can be:
        // in new_ids            : new
        // in old_values (was=X) : upd(was=X)
        // in removed (was=X)    : del(was=X)
        // not in any of above   : nop
        //
        // When merging A=prev_state and B=state we have a 4x4 matrix of all possibilities:
        //
        //                   |--------------------- B ----------------------|
        //
        //                +------------+------------+------------+------------+
        //                | new        | upd(was=Y) | del(was=Y) | nop        |
        //   +------------+------------+------------+------------+------------+
        // / | new        | N/A        | new       A| nop       C| new       A|
        // | +------------+------------+------------+------------+------------+
        // | | upd(was=X) | N/A        | upd(was=X)A| del(was=X)C| upd(was=X)A|
        // A +------------+------------+------------+------------+------------+
        // | | del(was=X) | N/A        | N/A        | N/A        | del(was=X)A|
        // | +------------+------------+------------+------------+------------+
        // \ | nop        | new       B| upd(was=Y)B| del(was=Y)B| nop      AB|
        //   +------------+------------+------------+------------+------------+
        //
        // Each entry was composed by labelling what should occur in the given case.
        //
        // Type A means the composition of states contains the same entry as the first of the two merged states for that object.
        // Type B means the composition of states contains the same entry as the second of the two merged states for that object.
        // Type C means the composition of states contains an entry different from either of the merged states for that object.
        // Type N/A means the composition of states violates causal timing.
        // Type AB means both type A and type B simultaneously.
        //
        // The merge() operation is defined as modifying prev_state in-place to be the state object which represents the composition of
        // state A and B.
        //
        // Type A (and AB) can be implemented as a no-op; prev_state already contains the correct value for the merged state.
        // Type B (and AB) can be implemented by copying from state to prev_state.
        // Type C needs special case-by-case logic.
        // Type N/A can be ignored or assert(false) as it can only occur if prev_state and state have illegal values
        // (a serious logic error which should never happen).
        //

        // We can only be outside type A/AB (the nop path) if B is not nop, so it suffices to iterate through B's three containers.

        for( auto& item : state.old_values )
        {
          if( prev_state.new_ids.find( item.second.get_id() ) != prev_state.new_ids.end() )
          {
            // new+upd -> new, type A
            continue;
          }
          if( prev_state.old_values.find( item.second.get_id() ) != prev_state.old_values.end() )
          {
            // upd(was=X) + upd(was=Y) -> upd(was=X), type A
            continue;
          }
          // del+upd -> N/A
          assert( prev_state.removed_values.find( item.second.get_id() ) == prev_state.removed_values.end() );
          // nop+upd(was=Y) -> upd(was=Y), type B
          prev_state.old_values.emplace( std::move(item) );
        }

        // *+new, but we assume the N/A cases don't happen, leaving type B nop+new -> new
        for( const auto& id : state.new_ids )
          prev_state.new_ids.insert(id);

        // *+del
        for( auto& obj : state.removed_values )
        {
          if( prev_state.new_ids.find( obj.second.get_id() ) != prev_state.new_ids.end() )
          {
            // new + del -> nop (type C)
            prev_state.new_ids.erase( obj.second.get_id() );
            continue;
          }
          auto it = prev_state.old_values.find( obj.second.get_id() );
          if( it != prev_state.old_values.end() )
          {
            // upd(was=X) + del(was=Y) -> del(was=X)
            prev_state.removed_values.emplace( std::move(*it) );
            prev_state.old_values.erase( obj.second.get_id() );
            continue;
          }
          // del + del -> N/A
          assert( prev_state.removed_values.find( obj.second.get_id() ) == prev_state.removed_values.end() );
          // nop + del(was=Y) -> del(was=Y)
          prev_state.removed_values.emplace( std::move(obj) ); //[obj.second->get_id()] = std::move(obj.second);
        }

        _stack.pop_back();
        --_revision;
      }

      /**
        * Discards all undo history prior to revision
        */
      void commit( int64_t revision )
      {
        while( _stack.size() && _stack[0].revision <= revision )
        {
          _stack.pop_front();
        }
      }

      /**
        * Unwinds all undo states
        */
      void undo_all()
      {
        while( enabled() )
          undo();
      }

      void set_revision( int64_t revision )
      {
        if( _stack.size() != 0 ) CHAINBASE_THROW_EXCEPTION( std::logic_error("cannot set revision while there is an existing undo stack") );
        _revision = revision;
      }

    private:
      bool enabled()const { return _stack.size(); }

      void on_modify( const value_type& v ) {
        if( !enabled() ) return;

        auto& head = _stack.back();

        if( head.new_ids.find( v.get_id() ) != head.new_ids.end() )
          return;

        auto itr = head.old_values.find( v.get_id() );
        if( itr != head.old_values.end() )
          return;

        head.old_values.emplace( v.get_id(), v.copy_chain_object() );
      }

      void on_remove( const value_type& v ) {
        if( !enabled() ) return;

        auto& head = _stack.back();
        if( head.new_ids.count( v.get_id() ) ) {
          head.new_ids.erase( v.get_id() );
          return;
        }

        auto itr = head.old_values.find( v.get_id() );
        if( itr != head.old_values.end() ) {
          head.removed_values.emplace( std::move( *itr ) );
          head.old_values.erase( v.get_id() );
          return;
        }

        if( head.removed_values.count( v.get_id() ) )
          return;

        head.removed_values.emplace( v.get_id(), v.copy_chain_object() );
      }

      void on_create( const value_type& v ) {
        if( !enabled() ) return;
        auto& head = _stack.back();

        head.new_ids.insert( v.get_id() );
      }

      boost::interprocess::deque< undo_state_type, allocator<undo_state_type> > _stack;

      /**
        *  Each new session increments the revision, a squash will decrement the revision by combining
        *  the two most recent revisions into one revision.
        *
        *  Commit will discard all revisions prior to the committed revision.
        */
      int64_t                         _revision = 0;
      id_type                         _next_id = id_type(0);
      index_type                      _indices;
      uint32_t                        _size_of_value_type = 0;
      uint32_t                        _size_of_this = 0;
  };

  class abstract_session {
    public:
      virtual ~abstract_session(){};
      virtual void push()             = 0;
      virtual void squash()           = 0;
      virtual void undo()             = 0;
      virtual int64_t revision()const  = 0;
  };

  template<typename SessionType>
  class session_impl : public abstract_session
  {
    public:
      session_impl( SessionType&& s ):_session( std::move( s ) ){}

      virtual void push() override  { _session.push();  }
      virtual void squash() override{ _session.squash(); }
      virtual void undo() override  { _session.undo();  }
      virtual int64_t revision()const override  { return _session.revision();  }
    private:
      SessionType _session;
  };

  class index_extension
  {
    public:
      index_extension() {}
      virtual ~index_extension() {}
  };

  typedef std::vector< std::shared_ptr< index_extension > > index_extensions;

  class abstract_index
  {
    public:
      typedef helpers::index_statistic_info statistic_info;

      abstract_index( void* i ):_idx_ptr(i){}
      virtual ~abstract_index(){}
      virtual void     set_revision( int64_t revision ) = 0;
      virtual unique_ptr<abstract_session> start_undo_session() = 0;

      virtual int64_t revision()const = 0;
      virtual void    undo()const = 0;
      virtual void    squash()const = 0;
      virtual void    commit( int64_t revision )const = 0;
      virtual void    undo_all()const = 0;
      virtual uint32_t type_id()const  = 0;

      virtual statistic_info get_statistics(bool onlyStaticInfo) const = 0;
      virtual size_t size() const = 0;
      virtual void clear() = 0;

      virtual void dump_snapshot(snapshot_writer& writer) const = 0;
      virtual void load_snapshot(snapshot_reader& reader) = 0;

      void add_index_extension( std::shared_ptr< index_extension > ext )  { _extensions.push_back( ext ); }
      const index_extensions& get_index_extensions()const  { return _extensions; }
      void* get()const { return _idx_ptr; }
    protected:
      void*              _idx_ptr;
    private:
      index_extensions   _extensions;
  };

  template<typename BaseIndex>
  class index_impl : public abstract_index {
    public:
      using abstract_index::statistic_info;

      index_impl( BaseIndex& base ):abstract_index( &base ),_base(base){}

      virtual unique_ptr<abstract_session> start_undo_session() override {
        return unique_ptr<abstract_session>(new session_impl<typename BaseIndex::session>( _base.start_undo_session() ) );
      }

      virtual void     set_revision( int64_t revision ) override { _base.set_revision( revision ); }
      virtual int64_t  revision()const  override { return _base.revision(); }
      virtual void     undo()const  override { _base.undo(); }
      virtual void     squash()const  override { _base.squash(); }
      virtual void     commit( int64_t revision )const  override { _base.commit(revision); }
      virtual void     undo_all() const override {_base.undo_all(); }
      virtual uint32_t type_id()const override { return BaseIndex::value_type::type_id; }

      virtual statistic_info get_statistics(bool onlyStaticInfo) const override final
      {
        typedef typename BaseIndex::index_type index_type;
        helpers::index_statistic_provider<index_type> provider;
        return provider.gather_statistics(_base.indices(), onlyStaticInfo);
      }

      virtual size_t size() const override final
      {
        return _base.indicies().size();
      }

      virtual void clear() override final
      {
        _base.clear();
      }

      virtual void dump_snapshot(snapshot_writer& writer) const override final
      {
        generic_index_snapshot_dumper<BaseIndex> dumper(_base, writer);
        dumper.dump(_base.get_next_id());
      }

      virtual void load_snapshot(snapshot_reader& reader) override final
      {
        clear();
        generic_index_snapshot_loader<BaseIndex> loader(_base, reader);
        auto next_id = loader.load();
        _base.store_next_id(next_id);
      }

    private:
      BaseIndex& _base;
  };

  template<typename IndexType>
  class index : public index_impl<IndexType> {
    public:
      index( IndexType& i ):index_impl<IndexType>( i ){}
  };

  struct lock_exception : public std::exception
  {
    explicit lock_exception() {}
    virtual ~lock_exception() {}

    virtual const char* what() const noexcept { return "Unable to acquire database lock"; }
  };

  /**
    *  This class
    */
  class database
  {
    private:
      class abstract_index_type
      {
        public:
          abstract_index_type() {}
          virtual ~abstract_index_type() {}

          virtual void add_index( database& db ) = 0;
      };

      template< typename IndexType >
      class index_type_impl : public abstract_index_type
      {
        virtual void add_index( database& db ) override
        {
          wlog("mtlk revision()= ${rev}", ("rev", db.revision()));

          db.add_index_helper< IndexType >();

          wlog("mtlk revision()= ${rev}", ("rev", db.revision()));

        }
      };

      void wipe_indexes();

    public:
      void open( const bfs::path& dir, uint32_t flags = 0, size_t shared_file_size = 0, const boost::any& database_cfg = nullptr, const helpers::environment_extension_resources* environment_extension = nullptr, const bool wipe_shared_file = false );
      void close();
      void flush();
      void wipe( const bfs::path& dir );
      void resize( size_t new_shared_file_size );
      void set_require_locking( bool enable_require_locking );

#ifdef CHAINBASE_CHECK_LOCKING
      void require_lock_fail( const char* method, const char* lock_type, const char* tname )const;

      void require_read_lock( const char* method, const char* tname )const
      {
        if( BOOST_UNLIKELY( _enable_require_locking && (_read_lock_count <= 0) ) )
          require_lock_fail(method, "read", tname);
      }

      void require_write_lock( const char* method, const char* tname )
      {
        if( BOOST_UNLIKELY( _enable_require_locking && (_write_lock_count <= 0) ) )
          require_lock_fail(method, "write", tname);
      }
#endif

      struct session {
        public:
          session( session&& s )
            : _index_sessions( std::move(s._index_sessions) ),
              _revision( s._revision ),
              _session_incrementer(std::move(s._session_incrementer))
          {}

          session( vector<std::unique_ptr<abstract_session>>&& s, int32_t& session_count )
            : _index_sessions( std::move(s) ), _session_incrementer( session_count )
          {
            if( _index_sessions.size() )
              _revision = _index_sessions[0]->revision();
          }

          ~session() {
            undo();
          }

          void push()
          {
            for( auto& i : _index_sessions ) i->push();
            _index_sessions.clear();
          }

          void squash()
          {
            for( auto& i : _index_sessions ) i->squash();
            _index_sessions.clear();
          }

          void undo()
          {
            for( auto& i : _index_sessions ) i->undo();
            _index_sessions.clear();
          }

          int64_t revision()const { return _revision; }

        private:
          friend class database;

          vector< std::unique_ptr<abstract_session> > _index_sessions;
          int64_t _revision = -1;
          session_int_incrementer _session_incrementer;
      };

      session start_undo_session();

      int64_t revision()const {
          if( _index_list.size() == 0 ) return -1;
          return _index_list[0]->revision();
      }

      void undo();
      void squash();
      void commit( int64_t revision );
      void undo_all();


      void set_revision( int64_t revision )
      {
          CHAINBASE_REQUIRE_WRITE_LOCK( "set_revision", int64_t );
          for( const auto& i : _index_list ) i->set_revision( revision );
      }

      template<typename MultiIndexType>
      void add_index()
      {
        _index_types.push_back( unique_ptr< abstract_index_type >( new index_type_impl< MultiIndexType >() ) );
        _index_types.back()->add_index( *this );
      }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnonnull"
      auto get_segment_manager() -> decltype( ((bip::managed_mapped_file*)nullptr)->get_segment_manager()) {
        return _segment->get_segment_manager();
      }
#pragma GCC diagnostic pop

      unsigned long long get_total_system_memory() const
      {
#if !defined( __APPLE__ ) // OS X does not support _SC_AVPHYS_PAGES
        long pages = sysconf(_SC_AVPHYS_PAGES);
        long page_size = sysconf(_SC_PAGE_SIZE);
        return pages * page_size;
#else
        return 0;
#endif
      }

      size_t get_free_memory()const
      {
        return _segment->get_segment_manager()->get_free_memory();
      }

      size_t get_max_memory()const
      {
        return _file_size;
      }

      template<typename MultiIndexType>
      bool has_index()const
      {
        CHAINBASE_REQUIRE_READ_LOCK("get_index", typename MultiIndexType::value_type);
        typedef generic_index<MultiIndexType> index_type;
        return _index_map.size() > index_type::value_type::type_id && _index_map[index_type::value_type::type_id];
      }

      template<typename MultiIndexType>
      const generic_index<MultiIndexType>& get_index()const
      {
        CHAINBASE_REQUIRE_READ_LOCK("get_index", typename MultiIndexType::value_type);
        typedef generic_index<MultiIndexType> index_type;
        typedef index_type*                   index_type_ptr;

        if( !has_index< MultiIndexType >() )
        {
          std::string type_name = boost::core::demangle( typeid( typename index_type::value_type ).name() );
          CHAINBASE_THROW_EXCEPTION( std::runtime_error( "unable to find index for " + type_name + " in database" ) );
        }

        return *index_type_ptr( _index_map[index_type::value_type::type_id]->get() );
      }

      template<typename MultiIndexType>
      void add_index_extension( std::shared_ptr< index_extension > ext )
      {
        typedef generic_index<MultiIndexType> index_type;

        if( !has_index< MultiIndexType >() )
        {
          std::string type_name = boost::core::demangle( typeid( typename index_type::value_type ).name() );
          CHAINBASE_THROW_EXCEPTION( std::runtime_error( "unable to find index for " + type_name + " in database" ) );
        }

        _index_map[index_type::value_type::type_id]->add_index_extension( ext );
      }

      template<typename MultiIndexType, typename ByIndex>
      auto get_index()const -> decltype( ((generic_index<MultiIndexType>*)( nullptr ))->indicies().template get<ByIndex>() )
      {
        CHAINBASE_REQUIRE_READ_LOCK("get_index", typename MultiIndexType::value_type);
        typedef generic_index<MultiIndexType> index_type;
        typedef index_type*                   index_type_ptr;

        if( !has_index< MultiIndexType >() )
        {
          std::string type_name = boost::core::demangle( typeid( typename index_type::value_type ).name() );
          CHAINBASE_THROW_EXCEPTION( std::runtime_error( "unable to find index for " + type_name + " in database" ) );
        }

        return index_type_ptr( _index_map[index_type::value_type::type_id]->get() )->indicies().template get<ByIndex>();
      }

      template<typename MultiIndexType>
      generic_index<MultiIndexType>& get_mutable_index()
      {
        CHAINBASE_REQUIRE_WRITE_LOCK("get_mutable_index", typename MultiIndexType::value_type);
        typedef generic_index<MultiIndexType> index_type;
        typedef index_type*                   index_type_ptr;

        if( !has_index< MultiIndexType >() )
        {
          std::string type_name = boost::core::demangle( typeid( typename index_type::value_type ).name() );
          CHAINBASE_THROW_EXCEPTION( std::runtime_error( "unable to find index for " + type_name + " in database" ) );
        }

        return *index_type_ptr( _index_map[index_type::value_type::type_id]->get() );
      }

      template< typename ObjectType, typename IndexedByType, typename CompatibleKey >
      const ObjectType* find( CompatibleKey&& key )const
      {
          CHAINBASE_REQUIRE_READ_LOCK("find", ObjectType);
          typedef typename get_index_type< ObjectType >::type index_type;
          const auto& idx = get_index< index_type >().indicies().template get< IndexedByType >();
          auto itr = idx.find( std::forward< CompatibleKey >( key ) );
          if( itr == idx.end() ) return nullptr;
          return &*itr;
      }

      template< typename ObjectType >
      const ObjectType* find( oid< ObjectType > key = oid_ref< ObjectType >() ) const
      {
          CHAINBASE_REQUIRE_READ_LOCK("find", ObjectType);
          typedef typename get_index_type< ObjectType >::type index_type;
          const auto& idx = get_index< index_type >().indices();
          auto itr = idx.find( key );
          if( itr == idx.end() ) return nullptr;
          return &*itr;
      }

      template< typename ObjectType, typename IndexedByType, typename CompatibleKey >
      const ObjectType& get( CompatibleKey&& key )const
      {
          CHAINBASE_REQUIRE_READ_LOCK("get", ObjectType);
          auto obj = find< ObjectType, IndexedByType >( std::forward< CompatibleKey >( key ) );
          if( !obj )
          {
            CHAINBASE_THROW_EXCEPTION( std::out_of_range( "unknown key" ) );
          }
          return *obj;
      }

      template< typename ObjectType >
      const ObjectType& get( const oid< ObjectType >& key = oid_ref< ObjectType >() )const
      {
          CHAINBASE_REQUIRE_READ_LOCK("get", ObjectType);
          auto obj = find< ObjectType >( key );
          if( !obj ) CHAINBASE_THROW_EXCEPTION( std::out_of_range( "unknown key") );
          return *obj;
      }

      template<typename ObjectType, typename Modifier>
      void modify( const ObjectType& obj, Modifier&& m )
      {
          CHAINBASE_REQUIRE_WRITE_LOCK("modify", ObjectType);
          typedef typename get_index_type<ObjectType>::type index_type;
          get_mutable_index<index_type>().modify( obj, std::forward<Modifier>( m ) );
      }

      template<typename ObjectType>
      void remove( const ObjectType& obj )
      {
          CHAINBASE_REQUIRE_WRITE_LOCK("remove", ObjectType);
          typedef typename get_index_type<ObjectType>::type index_type;
          return get_mutable_index<index_type>().remove( obj );
      }

      template<typename ObjectType, typename ... Args>
      const ObjectType& create( Args&&... args )
      {
          CHAINBASE_REQUIRE_WRITE_LOCK("create", ObjectType);
          typedef typename get_index_type<ObjectType>::type index_type;
          return get_mutable_index<index_type>().emplace( std::forward<Args>( args )... );
      }

      template< typename ObjectType >
      size_t count()const
      {
        typedef typename get_index_type<ObjectType>::type index_type;
        return get_index< index_type >().indices().size();
      }

      template< typename Lambda >
      auto with_read_lock( Lambda&& callback, fc::microseconds wait_for_microseconds = fc::microseconds() ) -> decltype( (*(Lambda*)nullptr)() )
      {
        uint32_t lock_serial_number = _next_read_lock_serial_number.fetch_add(1, std::memory_order_relaxed);
        fc_wlog(fc::logger::get("chainlock"), "trying to get chainbase_read_lock: read_lock_count=${_read_lock_count} write_lock_count=${_write_lock_count} (#${lock_serial_number})", 
                ("_read_lock_count", _read_lock_count.load(std::memory_order_relaxed))
                ("_write_lock_count", _write_lock_count.load(std::memory_order_relaxed))
                (lock_serial_number));
        read_lock lock(_rw_lock, boost::defer_lock_t());

#ifdef CHAINBASE_CHECK_LOCKING
        BOOST_ATTRIBUTE_UNUSED
        int_incrementer ii(_read_lock_count, "read", lock_serial_number);
#endif

        if (wait_for_microseconds == fc::microseconds())
          lock.lock();
        else if (!lock.try_lock_for(boost::chrono::microseconds(wait_for_microseconds.count())))
        {
          fc_wlog(fc::logger::get("chainlock"),"timedout getting chainbase_read_lock: read_lock_count=${_read_lock_count} write_lock_count=${_write_lock_count} (#${lock_serial_number})",
                  ("_read_lock_count", _read_lock_count.load(std::memory_order_relaxed))
                  ("_write_lock_count", _write_lock_count.load(std::memory_order_relaxed))
                  (lock_serial_number));
          CHAINBASE_THROW_EXCEPTION( lock_exception() );
        }

        fc_dlog(fc::logger::get("chainlock"), "_read_lock_count=${_read_lock_count} (#${lock_serial_number})", 
                ("_read_lock_count", _read_lock_count.load(std::memory_order_relaxed))
                (lock_serial_number));

        return callback();
      }

      template< typename Lambda >
      auto with_write_lock( Lambda&& callback ) -> decltype( (*(Lambda*)nullptr)() )
      {
        uint32_t lock_serial_number = _next_write_lock_serial_number.fetch_add(1, std::memory_order_relaxed);
        fc_wlog(fc::logger::get("chainlock"), "trying to get chainbase_write_lock: read_lock_count=${_read_lock_count} write_lock_count=${_write_lock_count} (#${lock_serial_number})", 
                ("_read_lock_count", _read_lock_count.load(std::memory_order_relaxed))
                ("_write_lock_count", _write_lock_count.load(std::memory_order_relaxed))
                (lock_serial_number));
        write_lock lock(_rw_lock, boost::defer_lock_t());
#ifdef CHAINBASE_CHECK_LOCKING
        BOOST_ATTRIBUTE_UNUSED
        int_incrementer ii(_write_lock_count, "write", lock_serial_number);
#endif

        lock.lock();
        fc_wlog(fc::logger::get("chainlock"),"got chainbase_write_lock: read_lock_count=${_read_lock_count} write_lock_count=${_write_lock_count} (#${lock_serial_number})",
                ("_read_lock_count", _read_lock_count.load(std::memory_order_relaxed))
                ("_write_lock_count", _write_lock_count.load(std::memory_order_relaxed))
                (lock_serial_number));

        return callback();
      }

      template< typename IndexExtensionType, typename Lambda >
      void for_each_index_extension( Lambda&& callback )const
      {
        for( const abstract_index* idx : _index_list )
        {
          const index_extensions& exts = idx->get_index_extensions();
          for( const std::shared_ptr< index_extension >& e : exts )
          {
            std::shared_ptr< IndexExtensionType > e2 = std::dynamic_pointer_cast< IndexExtensionType >( e );
            if( e2 )
              callback( e2 );
          }
        }
      }

      typedef vector<abstract_index*> abstract_index_cntr_t;

      const abstract_index_cntr_t& get_abstract_index_cntr() const
        { return _index_list; }

    protected:
      bool get_is_open() const
        { return _is_open; }

    private:
      template<typename MultiIndexType>
      void add_index_helper() {
        wlog("mtlk revision()= ${rev}", ("rev", revision()));

        const uint16_t type_id = generic_index<MultiIndexType>::value_type::type_id;
        typedef generic_index<MultiIndexType>          index_type;
        typedef typename index_type::allocator_type    index_alloc;

        wlog("mtlk revision()= ${rev}", ("rev", revision()));

        std::string type_name = boost::core::demangle( typeid( typename index_type::value_type ).name() );

        wlog("mtlk revision()= ${rev}", ("rev", revision()));

        if( !( _index_map.size() <= type_id || _index_map[ type_id ] == nullptr ) ) {
          CHAINBASE_THROW_EXCEPTION( std::logic_error( type_name + "::type_id is already in use" ) );
        }

        wlog("mtlk revision()= ${rev}", ("rev", revision()));

        index_type* idx_ptr =  nullptr;
        bool _is_index_new = false;
#ifdef ENABLE_STD_ALLOCATOR
        idx_ptr = new index_type( index_alloc() );
#else
        auto _found = _segment->find< index_type >( type_name.c_str() );
        if( !_found.first )
        {
          _is_index_new = true;
          idx_ptr = _segment->construct< index_type >( type_name.c_str() )( index_alloc( _segment->get_segment_manager() ) );
        }
        else
        {
          idx_ptr = _found.first;
        }
#endif

        wlog("mtlk revision()= ${rev}", ("rev", revision()));

        idx_ptr->validate();

        wlog("mtlk revision()= ${rev}", ("rev", revision()));

        if( _is_index_new )
          _at_least_one_index_is_created_now = true;
        else
          _at_least_one_index_was_created_earlier = true;

        if( _at_least_one_index_is_created_now && _at_least_one_index_was_created_earlier )
        {
          if( _is_index_new )
            CHAINBASE_THROW_EXCEPTION( std::logic_error( "Inconsistency occurs. A new index is created, but other indexes are found in `shared_memory_file` file. A replay is needed. Problem with: " + type_name ) );
          else
            CHAINBASE_THROW_EXCEPTION( std::logic_error( "Inconsistency occurs. A new index is found in `shared_memory_file` file, but other indexes are created. A replay is needed. Problem with: " + type_name ) );
        }

        wlog("mtlk revision()= ${rev}", ("rev", revision()));

        if( type_id >= _index_map.size() )
          _index_map.resize( type_id + 1 );

        wlog("mtlk revision()= ${rev}", ("rev", revision()));

        auto new_index = new index<index_type>( *idx_ptr );

        wlog("mtlk revision()= ${new_index}", ("new_index", new_index->revision()));
        wlog("mtlk revision()= ${rev}", ("rev", revision()));

        _index_map[ type_id ].reset( new_index );

        wlog("mtlk revision()= ${new_index}", ("new_index", new_index->revision()));
        wlog("mtlk revision()= ${rev}", ("rev", revision()));

        _index_list.push_back( new_index );

        wlog("mtlk revision()= ${new_index}", ("new_index", new_index->revision()));
        wlog("mtlk revision()= ${rev}", ("rev", revision()));

      }

      read_write_mutex                                            _rw_lock;

      unique_ptr<bip::managed_mapped_file>                        _segment;
      unique_ptr<bip::managed_mapped_file>                        _meta;
      bip::file_lock                                              _flock;

      /**
        * This is a sparse list of known indicies kept to accelerate creation of undo sessions
        */
      abstract_index_cntr_t                                       _index_list;

      /**
        * This is a full map (size 2^16) of all possible index designed for constant time lookup
        */
      vector<unique_ptr<abstract_index>>                          _index_map;

      vector<unique_ptr<abstract_index_type>>                     _index_types;

      bfs::path                                                   _data_dir;

      std::atomic<int32_t>                                        _read_lock_count = {0};
      std::atomic<int32_t>                                        _write_lock_count = {0};
      std::atomic<uint32_t>                                       _next_read_lock_serial_number = {0};
      std::atomic<uint32_t>                                       _next_write_lock_serial_number = {0};
      bool                                                        _enable_require_locking = false;

      bool                                                        _is_open = false;

      int32_t                                                     _undo_session_count = 0;
      size_t                                                      _file_size = 0;
      boost::any                                                  _database_cfg = nullptr;

      bool                                                        _at_least_one_index_was_created_earlier = false;
      bool                                                        _at_least_one_index_is_created_now = false;
  };

}  // namepsace chainbase

