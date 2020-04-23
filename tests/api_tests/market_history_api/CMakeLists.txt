# Adding python tests to cmake
MESSAGE( STATUS "### Adding market_history_api python tests")
#### get_market_history
ADD_API_TEST(market_history_api get_market_history 86400 2019-01-01T00:00:00 2019-01-02T00:00:00)

#### get_market_history_buckets
ADD_API_TEST(market_history_api get_market_history_buckets)

#### get_order_book
ADD_API_TEST(market_history_api get_order_book 20)

#### get_recent_trades
ADD_API_TEST(market_history_api get_recent_trades 20)

#### get_ticker
ADD_API_TEST(market_history_api get_ticker)

#### get_trade_history
ADD_API_TEST(market_history_api get_trade_history 2018-01-01T00:00:00 2019-01-02T00:00:00 20)

#### get_volume
ADD_API_TEST(market_history_api get_volume)
