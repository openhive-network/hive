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
-- -- Execute theese commands with proper rights, and comment following 3 lines
DROP SCHEMA public CASCADE;
CREATE SCHEMA public;
CREATE EXTENSION IF NOT EXISTS intarray;

-- -- Check current status
-- SELECT COUNT(*) FROM hive_blocks;
-- SELECT COUNT(*) FROM hive_transactions;
-- SELECT COUNT(*) FROM hive_operations;
-- SELECT COUNT(*) FROM hive_virtual_operations;
-- SELECT COUNT(*) FROM hive_accounts;
-- SELECT COUNT(*) FROM hive_operation_types;
-- SELECT COUNT(*) FROM hive_permlink_data;

-- -- Drop tables
-- DROP TABLE IF EXISTS hive_blocks;
-- DROP TABLE IF EXISTS hive_transactions;
-- DROP TABLE IF EXISTS hive_operations;
-- DROP TABLE IF EXISTS hive_virtual_operations;
-- DROP TABLE IF EXISTS hive_accounts;
-- DROP TABLE IF EXISTS hive_operation_types;
-- DROP TABLE IF EXISTS hive_permlink_data;

-- -- Core Tables

CREATE TABLE IF NOT EXISTS hive_blocks (
  "num" integer NOT NULL,
  "hash" bytea NOT NULL,
  "created_at" timestamp without time zone NOT NULL,
  CONSTRAINT hive_blocks_pkey PRIMARY KEY ("num"),
  CONSTRAINT hive_blocks_uniq UNIQUE ("hash")
);

CREATE TABLE IF NOT EXISTS hive_transactions (
  "block_num" integer NOT NULL,
  "trx_in_block" smallint NOT NULL,
  "trx_hash" bytea NOT NULL,
  CONSTRAINT hive_transactions_pkey PRIMARY KEY ("block_num", "trx_in_block"),
  CONSTRAINT hive_transactions_uniq_1 UNIQUE ("trx_hash")
);

CREATE TABLE IF NOT EXISTS hive_operation_types (
  -- This "id", due to filling logic is not autoincrement, but is set by `fc::static_variant::which()` (a.k.a.. operation )
  "id" smallint NOT NULL,
  "name" text NOT NULL,
  "is_virtual" boolean NOT NULL,
  CONSTRAINT hive_operation_types_pkey PRIMARY KEY ("id")
);

CREATE TABLE IF NOT EXISTS hive_permlink_data (
  "id" serial,
  "permlink" varchar(255) NOT NULL,
  CONSTRAINT hive_permlink_data_pkey PRIMARY KEY ("id"),
  CONSTRAINT hive_permlink_data_uniq UNIQUE ("permlink")
);

CREATE TABLE IF NOT EXISTS hive_operations (
  "block_num" integer NOT NULL,
  "trx_in_block" smallint NOT NULL,
  "op_pos" smallint NOT NULL,
  "op_type_id" smallint NOT NULL,
  "body" text DEFAULT NULL,
  "permlink_ids" int[],
  -- Participants is array of hive_accounts.id, which stands for accounts that participates in selected operation
  "participants" int[],
  CONSTRAINT hive_operations_pkey PRIMARY KEY ("block_num", "trx_in_block", "op_pos"),
  CONSTRAINT hive_operations_unsigned CHECK ("trx_in_block" >= 0 AND "op_pos" >= 0)
);

CREATE TABLE IF NOT EXISTS hive_accounts (
  "id" serial,
  "name" character (16) NOT NULL,
  CONSTRAINT hive_accounts_pkey PRIMARY KEY ("id"),
  CONSTRAINT hive_accounts_uniq UNIQUE ("name")
);

CREATE TABLE IF NOT EXISTS hive_virtual_operations (
  -- just to keep them unique
  "id" serial,
  "block_num" integer NOT NULL,
  "trx_in_block" smallint NOT NULL,
  -- for `trx_in_block` = -1, `op_pos` stands for order
  "op_pos" integer NOT NULL,
  "op_type_id" smallint NOT NULL,
  "body" text DEFAULT NULL,
  -- Participants is array of hive_accounts.id, which stands for accounts that participates in selected operation
  "participants" int[],
  CONSTRAINT hive_virtual_operations_pkey PRIMARY KEY ("id")
);

-- SPECIAL VALUES
-- This is permlink referenced by empty permlink arrays
INSERT INTO hive_permlink_data VALUES(0, '');
-- This is account referenced by empty participants arrays
INSERT INTO hive_accounts VALUES(0, '');
