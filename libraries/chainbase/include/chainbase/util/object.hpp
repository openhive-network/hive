#pragma once

#include <chainbase/util/object_id.hpp>
#include <chainbase/allocator_helpers.hpp>

#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/facilities/overload.hpp>

#include <functional>

namespace chainbase {

template<typename MultiIndexType> class generic_index;

template<uint16_t TypeNumber, typename Derived, typename HasDynamicAlloc = std::false_type, typename EnableNoUndo = std::false_type>
struct object
{
  typedef oid<Derived> id_type;
  typedef oid_ref<Derived> id_ref_type;
  enum { type_id = TypeNumber };
  /*
  Mark chain object class with true_type when it has elements that need to be passed an allocator.
  It adds code that collects dynamic allocation for benchmarks.
  */
  typedef HasDynamicAlloc has_dynamic_alloc_t;
  /*
  Do not mark chain object class with it unless you know what you are doing! It enables "no undo"
  destructions of objects, but optimized for very specific scenario, where you are guaranteed that
  creation/modification of object prior of its "no undo" destruction is not present on different
  active revision of undo stack. If used in different scenario, rare forking events can lead to
  crashes or inconsistent state.
  Example: comment_object is not modified and there is 7 day grace period between creation and destruction
  during migration to archive, so there is no waiting undo element when migration is performed;
  the same is not true for volatile_comment_object
  */
  typedef EnableNoUndo enable_no_undo_t;
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

} // namespace chainbase
