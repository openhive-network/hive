#!/bin/bash

sudo -Hiu postgres psql -d template1 -c "CREATE DATABASE haf_block_log;"
sudo -Hiu postgres psql -d haf_block_log -c "CREATE EXTENSION hive_fork_manager;"
sudo -Hiu postgres psql -d haf_block_log -c "CREATE ROLE myuser LOGIN PASSWORD 'mypassword' INHERIT IN ROLE hived_group;"
sudo -Hiu postgres psql -d haf_block_log -c "ALTER DATABASE haf_block_log OWNER TO myuser";

exit 0
