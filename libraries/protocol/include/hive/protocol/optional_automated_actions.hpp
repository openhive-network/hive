#pragma once

#include <hive/protocol/hive_optional_actions.hpp>

#include <hive/protocol/operation_util.hpp>

namespace hive { namespace protocol {

  /** NOTE: do not change the order of any actions or it will trigger a hardfork.
    */
  typedef fc::static_variant<
#ifdef IS_TEST_NET
        example_optional_action
#endif
      > optional_automated_action;

} } // hive::protocol

HIVE_DECLARE_OPERATION_TYPE( hive::protocol::optional_automated_action );

FC_TODO( "Remove ifdef when first optional automated action is added" )
#ifdef IS_TEST_NET
FC_REFLECT_TYPENAME( hive::protocol::optional_automated_action );
#endif