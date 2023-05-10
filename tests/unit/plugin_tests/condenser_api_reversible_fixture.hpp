#pragma once

#if defined IS_TEST_NET

#include "condenser_api_fixture.hpp"

// Prepares multi-witness environment for testing reversible blocks.
// Generates extra block with witness voting operations.
struct condenser_api_reversible_fixture : condenser_api_fixture
{
  condenser_api_reversible_fixture();
  virtual ~condenser_api_reversible_fixture();
};

#endif
