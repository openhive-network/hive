-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
-- -- -- -- -- -- -- -- W A R N I N G- -- -- -- -- -- -- -- -- 
-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
-- Because of security reasons, queries cannot be executed  -- 
-- one after another sepraeted with semicolon.              -- 
-- Use `normalize_schema.py <THIS FILE> <NEW FILE>          -- 
-- and set `psql-path-to-schema` to <NEW FILE>              -- 
-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 

-- -- Create Database
-- CREATE DATABASE block_log;

-- -- Backups
-- -- Delete backup
-- DROP DATABASE IF EXISTS block_log_back;
-- -- Create backup
-- CREATE DATABASE block_log_back WITH TEMPLATE block_log;
-- -- Restore backup
-- DROP DATABASE block_log;
-- CREATE DATABASE block_log WITH TEMPLATE block_log_back;

-- -- Reset
-- -- NOTE:
-- -- This section may require additional rights
-- -- If user, that is in connection string doesn't have proper acces rights
-- -- Execute theese commands with proper rights, comment 
-- -- following 3 lines, and uncomment theese under `Drop tables`
--DROP SCHEMA public CASCADE;
--CREATE SCHEMA public;
--CREATE EXTENSION IF NOT EXISTS intarray;

-- -- Check current status
-- SELECT COUNT(*) FROM hive_blocks;
-- SELECT COUNT(*) FROM hive_transactions;
-- SELECT COUNT(*) FROM hive_operations;
-- SELECT COUNT(*) FROM hive_virtual_operations;
-- SELECT COUNT(*) FROM hive_accounts;
-- SELECT COUNT(*) FROM hive_operation_types;
-- SELECT COUNT(*) FROM hive_permlink_data;

-- -- Drop tables
-- DROP TABLE IF EXISTS hive_blocks CASCADE;
-- DROP TABLE IF EXISTS hive_transactions CASCADE;
-- DROP TABLE IF EXISTS hive_operations CASCADE;
-- DROP TABLE IF EXISTS hive_virtual_operations CASCADE;
-- DROP TABLE IF EXISTS hive_accounts CASCADE;
-- DROP TABLE IF EXISTS hive_operation_types CASCADE;
-- DROP TABLE IF EXISTS hive_permlink_data CASCADE;

-- -- Core Tables

CREATE TABLE IF NOT EXISTS hive_blocks (
  "num" integer NOT NULL,
  "hash" bytea NOT NULL,
  "created_at" timestamp without time zone NOT NULL
  --,CONSTRAINT hive_blocks_pkey PRIMARY KEY ("num"),
  --CONSTRAINT hive_blocks_uniq UNIQUE ("hash")
);

CREATE TABLE IF NOT EXISTS hive_transactions (
  "block_num" integer NOT NULL,
  "trx_in_block" smallint NOT NULL,
  "trx_hash" bytea NOT NULL
  --,CONSTRAINT hive_transactions_pkey PRIMARY KEY ("block_num", "trx_in_block"),
  --CONSTRAINT hive_transactions_uniq_1 UNIQUE ("trx_hash")
);

CREATE TABLE IF NOT EXISTS hive_operation_types (
  -- This "id", due to filling logic is not autoincrement, but is set by `fc::static_variant::which()` (a.k.a.. operation )
  "id" smallint NOT NULL,
  "name" text NOT NULL,
  "is_virtual" boolean NOT NULL
  --,CONSTRAINT hive_operation_types_pkey PRIMARY KEY ("id")
);

CREATE TABLE IF NOT EXISTS hive_permlink_data (
  id INTEGER NOT NULL,
  permlink varchar(255) NOT NULL
  --,CONSTRAINT hive_permlink_data_pkey PRIMARY KEY ("id"),
  --CONSTRAINT hive_permlink_data_uniq UNIQUE ("permlink")
);

--- Stores all operation definitions (regular like also virtual ones).
CREATE TABLE IF NOT EXISTS hive_operations (
  id bigint not null,
  block_num integer NOT NULL,
  trx_in_block smallint NOT NULL,
  op_pos smallint NOT NULL,
  op_type_id smallint NOT NULL,
  body text DEFAULT NULL
  --,permlink_ids int[]
  --,CONSTRAINT hive_operations_pkey PRIMARY KEY (id) --("block_num", "trx_in_block", "op_pos")
);

CREATE TABLE IF NOT EXISTS hive_accounts (
  id INTEGER NOT NULL,
  name VARCHAR(16) NOT NULL
  --,CONSTRAINT hive_accounts_pkey PRIMARY KEY ("id"),
  --CONSTRAINT hive_accounts_uniq UNIQUE ("name")
);

CREATE TABLE IF NOT EXISTS hive_account_operations
(
  --- Identifier of account involved in given operation.
  account_id integer not null,
  --- Operation sequence number specific to given account. 
  account_op_seq_no integer not null,
  --- Id of operation held in hive_opreations table.
  operation_id bigint not null
);

DROP VIEW IF EXISTS account_operation_count_info_view CASCADE;
CREATE OR REPLACE VIEW account_operation_count_info_view
AS
SELECT ha.id, ha.name,
       COALESCE((SELECT COUNT(ao.account_op_seq_no) FROM hive_account_operations ao
        WHERE ao.account_id = ha.id
        GROUP BY ao.account_id), 0) as operation_count
FROM hive_accounts ha
;

-- SPECIAL VALUES
-- This is permlink referenced by empty permlink arrays
INSERT INTO hive_permlink_data VALUES(0, '');
-- This is account referenced by empty participants arrays
INSERT INTO hive_accounts VALUES(0, '');

DROP TYPE IF EXISTS hive_api_operation CASCADE;
CREATE TYPE hive_api_operation AS (
    id BIGINT,
    block_num INT,
    operation_type_id SMALLINT,
    is_virtual BOOLEAN,
    body VARCHAR
);

CREATE OR REPLACE FUNCTION enum_operations4hivemind(in _first_block INT, in _last_block INT)
RETURNS SETOF hive_api_operation
AS
$function$
BEGIN
  RETURN QUERY -- enum_operations4hivemind
    SELECT ho.id, ho.block_num, ho.op_type_id, ho.op_type_id >= 48 AS is_virtual, ho.body::VARCHAR
    FROM hive_operations ho
    WHERE ho.block_num between _first_block and _last_block
          AND (ho.op_type_id < 48 -- (select t.id from hive_operation_types t where t.is_virtual order by t.id limit 1)
               OR ho.op_type_id in (49, 51, 59, 70, 71)
              )
    ORDER BY ho.id
    /*
select id from hive_operation_types where is_virtual 
and name in ('hive::protocol::author_reward_operation',
'hive::protocol::comment_reward_operation',
'hive::protocol::effective_comment_vote_operation',
'hive::protocol::comment_payout_update_operation',
'hive::protocol::ineffective_delete_comment_operation')
*/
; 

END
$function$
LANGUAGE plpgsql STABLE
;

