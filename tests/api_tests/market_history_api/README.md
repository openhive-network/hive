# Market History Api 
- get_market_history
- get_market_history_buckets
- get_order_book
- get_recent_trades
- get_ticker
- get_trade_history
- get_volume

#### get_market_history
###### Returns the market history for the internal HBD:HIVE market.
Example call:

`python get_market_history.py https://api.steemit.com https://api.steem.house ./ 
86400 2019-01-01T00:00:00 2019-01-02T00:00:00`

Result: no differences.

#### get_market_history_buckets
###### Returns the bucket seconds being tracked by the plugin.
Example call: 

`python get_market_history_buckets.py  https://api.steemit.com https://api.steem.house ./`

Result: no differences.

#### get_order_book
###### Returns the internal market order book.
Example call:

`python get_order_book.py https://api.steemit.com https://api.steem.house ./ 20`

Result: no differences.

#### get_recent_trades
###### Returns the most recent trades for the internal HBD:HIVE market.
Example call:

`python get_recent_trades.py https://api.steemit.com https://api.steem.house ./ 20`

Result: no differences.

#### get_ticker
###### Returns the market ticker for the internal HBD:HIVE market.
Example call:

`python get_ticker.py https://api.steemit.com https://api.steem.house ./`

Result: no differences.

#### get_trade_history
###### Returns the trade history for the internal HBD:HIVE market.
Example call:

`python get_trade_history.py https://api.steemit.com https://api.steem.house ./ 
2018-01-01T00:00:00 2019-01-02T00:00:00 20`

Result: no differences.
 
#### get_volume
###### Returns the market volume for the past 24 hours.
Example call:

`python get_volume.py https://api.steemit.com https://api.steem.house ./`

Result: no differences.