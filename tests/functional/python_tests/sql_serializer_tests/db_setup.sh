#!/bin/bash

echo $DB_NAME
echo $DB_USER
echo $DB_PASSWORD
psql -d template1 -c "CREATE DATABASE haf_block_log;"
psql -d haf_block_log -c "CREATE EXTENSION hive_fork_manager;"
psql -d haf_block_log -c "CREATE ROLE myuser LOGIN PASSWORD 'mypassword' INHERIT IN ROLE hived_group;"
psql -d haf_block_log -c "ALTER DATABASE haf_block_log OWNER TO myuser";

exit 0
