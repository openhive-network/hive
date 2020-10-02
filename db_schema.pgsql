DROP DATABASE xd

CREATE DATABASE block_log;

CREATE EXTENSION IF NOT EXISTS intarray WITH SCHEMA public;

DROP TABLE hive_blocks;
CREATE TABLE IF NOT EXISTS hive_blocks (
    num INTEGER NOT NULL,
    hash CHARACTER (40) NOT NULL,
    created_at timestamp WITHOUT time zone NOT NULL
);

DROP TABLE hive_transactions;
CREATE TABLE IF NOT EXISTS hive_transactions (
    block_num INTEGER NOT NULL,
    trx_pos INTEGER NOT NULL,
    trx_hash character (40) NOT NULL
);

DROP TABLE hive_operation_names;
CREATE TABLE IF NOT EXISTS hive_operation_names (
    id INTEGER NOT NULL,
    "name" TEXT NOT NULL
);

DROP TABLE hive_operations;
CREATE TABLE IF NOT EXISTS hive_operations (
    block_num INTEGER NOT NULL,
    trx_pos INTEGER NOT NULL,
    op_pos INTEGER NOT NULL,
    op_name_id INTEGER NOT NULL,
    is_virtual boolean NOT NULL DEFAULT FALSE,
    participants char(16)[]
);

DROP TABLE hive_asset_dictionary;
CREATE TABLE IF NOT EXISTS hive_asset_dictionary (
    "asset_id" INTEGER NOT NULL,
    "precision" smallint NOT NULL,
    "symbol" CHARACTER(5) NOT NULL,
    "nai" TEXT NOT NULL
);

DROP TABLE hive_operation_details_asset;
CREATE TABLE IF NOT EXISTS hive_operation_details_asset (
    block_num INTEGER NOT NULL,
    trx_pos INTEGER NOT NULL,
    op_pos INTEGER NOT NULL,
    "amount" BIGINT NOT NULL,
    "asset_id" INTEGER NOT NULL
);

DROP TABLE hive_permlink_dictionary;
CREATE TABLE IF NOT EXISTS hive_permlink_dictionary (
    "permlink_id" INTEGER NOT NULL,
    "permlink" TEXT NOT NULL
);

DROP TABLE hive_operation_details_permlink;
CREATE TABLE IF NOT EXISTS hive_operation_details_permlink (
    block_num INTEGER NOT NULL,
    trx_pos INTEGER NOT NULL,
    op_pos INTEGER NOT NULL,
    "permlink" TEXT NOT NULL
);

DROP TABLE hive_operation_details_id;
CREATE TABLE IF NOT EXISTS hive_operation_details_id (
    block_num INTEGER NOT NULL,
    trx_pos INTEGER NOT NULL,
    op_pos INTEGER NOT NULL,
    "id" INTEGER NOT NULL
);

-- CREATE TABLE IF NOT EXISTS hive_accounts (
--     id INTEGER NOT NULL,
--     "name" TEXT NOT NULL,
--     "created_at_block" INTEGER NOT NULL
-- );

DROP TABLE IF EXISTS hive_transactions;


TRUNCATE hive_transactions;
TRUNCATE hive_blocks;
TRUNCATE hive_operations;
TRUNCATE hive_operation_names;

SELECT MAX(block_num) FROM hive_operations GROUP BY is_virtual HAVING is_virtual=true;

SELECT COUNT(*) as operations FROM hive_operations;
SELECT COUNT(*) as trx FROM hive_transactions;
SELECT COUNT(*) as blocks FROM hive_blocks;
SELECT * FROM hive_blocks ORDER BY num LIMIT 1000;
SELECT * FROM hive_operations WHERE 'gandalf'= ANY(participants) AND is_virtual=true LIMIT 1000;
SELECT COUNT(*) FROM hive_operations WHERE is_virtual=false;
SELECT COUNT(*) FROM hive_operations WHERE is_virtual=true;
SELECT * FROM hive_operations WHERE is_virtual=true ORDER BY block_num DESC LIMIT 100;
SELECT COUNT(*) FROM hive_operations;

SELECT * FROM hive_operation_names WHERE name='vote_operation'
INSERT INTO hive_operation_names VALUES ( 79, 'vote_operation' )



SELECT * FROM hive_operations WHERE "op_name_id"=3 LIMIT 100;

curl -s --data '{"jsonrpc":"2.0", "method":"account_history_api.dump_to_postgres", "params":{"db_host":"127.0.0.1", "db_user":"postgres", "db_pass":"pass", "db_name":"block_log", "db_port":5432, "block_range_begin":1, "block_range_end":1000000}, "id":1}' http://127.0.0.1:8091