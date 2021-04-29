#pragma once

namespace hive { namespace converter {

  struct convert_operations_visitor
  {
    typedef operation result_type;

    // No signatures modification ops
    template< typename T >
    const T& operator()( const T& op )const { return op; }
  };

} }