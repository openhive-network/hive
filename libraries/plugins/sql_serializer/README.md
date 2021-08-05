# SQL serializer plugin

## Before cmake

	sudo apt-get install libpqxx-dev -y

## Basic setup

Add this to your `config.ini`, with proper data

```
plugin = sql_serializer
psql-url = dbname=block_log user=postgres password=pass hostaddr=127.0.0.1 port=5432
psql-index-threshold = 1000000
```

### Example command

	./hived --replay-blockchain --stop-replay-at-block 5000000 --exit-after-replay -d ../../../datadir --force-replay --psql-index-threshold 65432