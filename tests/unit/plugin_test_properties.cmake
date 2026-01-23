# Test properties for plugin_test - set COST for CTest scheduling (higher cost = starts first)
# This file is included after test discovery to set properties on dynamically discovered tests

include("${CMAKE_CURRENT_LIST_DIR}/BoostTestPropertiesMacros.cmake")

set_test_properties_if_exists(unit/plugin_test-market_history/mh_test PROPERTIES COST 142)
