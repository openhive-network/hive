#pragma once

#include <cstdint>
#include <cstdlib>

namespace fc
{
template <typename T>
class reflector;
}

namespace chainbase
{

/**
*  Object ID type that includes the type of the object it references.
*  Each class of chain object has its own ID type and separate object counter.
*/
template<typename T>
class oid
{
  using __id_type = uint32_t;
  __id_type _id;

public:
  explicit oid( __id_type i ) : _id( i ) {} //lack of default makes sure all chain-object constructors fill their id members

  oid& operator++() { ++_id; return *this; }

  __id_type get_value() const { return _id; }
  operator size_t () const { return static_cast<size_t>(_id); }

  static oid<T> null_id() noexcept
  {
    return oid<T>(std::numeric_limits<__id_type>::max());
  }

  static oid<T> start_id() noexcept
  {
    return oid<T>(std::numeric_limits<__id_type>::min());
  }

  bool operator < (const oid & rhs) const { return _id < rhs._id; }
  bool operator > (const oid& rhs) const { return _id > rhs._id; }
  bool operator == (const oid& rhs) const { return _id == rhs._id; }
  bool operator != (const oid& rhs) const { return _id != rhs._id; }

  friend class fc::reflector< oid<T> >;
};

/**
*  Reference to object ID.
*  Contrary to object ID itself this one has default constructor, but can only be created from object ID
*/
template<typename T>
class oid_ref : public oid<T>
{
public:
  oid_ref() : oid<T>( 0 ) {}
  oid_ref( const oid<T>& id ) : oid<T>( id ) {}
  oid_ref& operator= ( const oid<T>& id ) { *static_cast< oid<T>* >( this ) = id; return *this; }
};

struct by_id;

} /// namespace chainbase
