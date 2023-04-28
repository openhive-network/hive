#pragma once

#if defined IS_TEST_NET

#include "condenser_api_fixture.hpp"

struct condenser_api_reversible_fixture : condenser_api_fixture
{
  condenser_api_reversible_fixture();
  virtual ~condenser_api_reversible_fixture();
};

#endif
