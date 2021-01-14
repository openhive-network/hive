-- CREATE DATABASE block_log_3;

-- -- Backups
-- CREATE DATABASE block_log_back
-- WITH TEMPLATE block_log_3;
-- DROP DATABASE block_log_back;
-- DROP DATABASE block_log_3;
-- CREATE DATABASE block_log_3
-- WITH TEMPLATE block_log_back;

-- -- Reset
DROP SCHEMA public CASCADE;
CREATE SCHEMA public;
CREATE EXTENSION IF NOT EXISTS intarray;


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
-- -- Cache whole table, as this will be very often accessed and it's quite small
-- -- TODO: fill whole table on start

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
  -- for `trx_in_block` = 1, `op_pos` stands for order
  "op_pos" integer NOT NULL,
  "op_type_id" smallint NOT NULL,
  "body" text DEFAULT NULL,
  -- Participants is array of hive_accounts.id, which stands for accounts that participates in selected operation
  "participants" int[],
  CONSTRAINT hive_virtual_operations_pkey PRIMARY KEY ("id")
);

DROP FUNCTION IF EXISTS insert_operation_type_id;
CREATE OR REPLACE FUNCTION insert_operation_type_id (integer, text, boolean) RETURNS integer AS $func$
DECLARE
	tmp INTEGER;
BEGIN
	SELECT id INTO tmp FROM hive_operation_types WHERE "name" = $2;
	IF NOT FOUND THEN
			INSERT INTO hive_operation_types VALUES ($1, $2, $3) RETURNING id INTO tmp;
	END IF;
	RETURN (SELECT tmp);
END
$func$
LANGUAGE 'plpgsql';

-- SPECIAL VALUES
INSERT INTO hive_permlink_data VALUES(0, '');	-- This is permlink referenced by empty permlink arrays
INSERT INTO hive_accounts VALUES(0, '');	-- This is account referenced by empty participants arrays


SELECT COUNT(*) FROM hive_blocks