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
-- CREATE EXTENSION IF NOT EXISTS pg_prewarm;


-- -- Core Tables
CREATE TABLE IF NOT EXISTS hive_blocks (
  "num" integer NOT NULL,
  "hash" character (40) NOT NULL,
  CONSTRAINT hive_blocks_pkey PRIMARY KEY ("num") NOT DEFERRABLE,
  CONSTRAINT hive_blocks_uniq UNIQUE ("hash") NOT DEFERRABLE
);

CREATE TABLE IF NOT EXISTS hive_transactions (
  "block_num" integer NOT NULL,
  "trx_in_block" integer NOT NULL,
  "trx_hash" character (40) NOT NULL,
  CONSTRAINT hive_transactions_pkey PRIMARY KEY ("block_num", "trx_in_block") NOT DEFERRABLE,
  CONSTRAINT hive_transactions_uniq_1 UNIQUE ("trx_hash") DEFERRABLE,
  CONSTRAINT hive_transactions_fk_1 FOREIGN KEY ("block_num") REFERENCES hive_blocks ("num") DEFERRABLE
);

CREATE TABLE IF NOT EXISTS hive_operation_types (
  -- This "id", due to filling logic is not autoincrement, but is set by `fc::static_variant::which()` (a.k.a.. operation )
  "id" integer NOT NULL,
  "name" text NOT NULL,
  "is_virtual" boolean NOT NULL,
  CONSTRAINT hive_operation_types_pkey PRIMARY KEY ("id") NOT DEFERRABLE
);
-- -- Cache whole table, as this will be very often accessed and it's quite small
-- -- TODO: fill whole table on start
-- SELECT pg_prewarm('hive_operation_types');

CREATE TABLE IF NOT EXISTS hive_permlink_data (
  "id" serial,
  "permlink" text NOT NULL,
  CONSTRAINT hive_permlink_dictionary_pkey PRIMARY KEY ("id") NOT DEFERRABLE,
  CONSTRAINT hive_permlink_dictionary_uniq UNIQUE ("permlink") NOT DEFERRABLE
);

CREATE TABLE IF NOT EXISTS hive_operations (
  "id" serial,
  "block_num" integer NOT NULL,
  "trx_in_block" integer NOT NULL,
  "op_pos" integer NOT NULL,
  "op_type_id" integer NOT NULL,
  -- Participants is array of hive_accounts.id, which stands for accounts that participates in selected operation
  "participants" int[],
  "permlink_ids" int[],
  body text DEFAULT NULL,
  CONSTRAINT hive_operations_pkey PRIMARY KEY ("id") NOT DEFERRABLE,
  CONSTRAINT hive_operations_uniq UNIQUE ("block_num", "trx_in_block", "op_pos") DEFERRABLE,
  CONSTRAINT hive_operations_unsigned CHECK ("trx_in_block" >= 0 AND "op_pos" >= 0),
  CONSTRAINT hive_operations_fk_1 FOREIGN KEY ("op_type_id") REFERENCES hive_operation_types ("id") DEFERRABLE,
  CONSTRAINT hive_operations_fk_2 FOREIGN KEY ("block_num", "trx_in_block") REFERENCES hive_transactions ("block_num", "trx_in_block") DEFERRABLE
);

CREATE TABLE IF NOT EXISTS hive_accounts (
  "id" serial,
  "name" character (40) NOT NULL,
  CONSTRAINT hive_accounts_pkey PRIMARY KEY ("id") NOT DEFERRABLE,
  CONSTRAINT hive_accounts_uniq UNIQUE ("name") NOT DEFERRABLE
);

CREATE TABLE IF NOT EXISTS hive_virtual_operations (
  "id" serial,
  "block_num" integer NOT NULL,
  "trx_in_block" integer,
  "op_pos" integer,
  "op_type_id" integer NOT NULL,
  -- Participants is array of hive_accounts.id, which stands for accounts that participates in selected operation
  "participants" int[],
  body text DEFAULT NULL,
  CONSTRAINT hive_virtual_operations_pkey PRIMARY KEY ("id") NOT DEFERRABLE,
  CONSTRAINT hive_virtual_operations_fk_1 FOREIGN KEY ("op_type_id") REFERENCES hive_operation_types ("id") DEFERRABLE,
  CONSTRAINT hive_virtual_operations_fk_2 FOREIGN KEY ("block_num") REFERENCES hive_blocks ("num") DEFERRABLE
);

-- -- Cache accessors
DROP FUNCTION IF EXISTS get_inserted_permlink_id;
CREATE OR REPLACE FUNCTION get_inserted_permlink_id (text) RETURNS integer AS $func$
BEGIN
	RETURN (SELECT tid FROM tmp_legacy_permlinks WHERE "permlink" = $1);
END
$func$
LANGUAGE 'plpgsql';

DROP FUNCTION IF EXISTS get_inserted_account_id;
CREATE OR REPLACE FUNCTION get_inserted_account_id (text) RETURNS integer AS $func$
BEGIN
	RETURN (SELECT tid FROM tmp_legacy_accounts WHERE "acc" = $1);
END
$func$
LANGUAGE 'plpgsql';

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


-- -- Cache creators
CREATE OR REPLACE FUNCTION prepare_permlink_cache () RETURNS VOID AS $func$

	CREATE TEMPORARY TABLE IF NOT EXISTS tmp_permlinks( permlink TEXT UNIQUE ) ON COMMIT DELETE ROWS;
	CREATE TEMPORARY TABLE IF NOT EXISTS tmp_legacy_permlinks( permlink TEXT UNIQUE, tid INTEGER ) ON COMMIT DELETE ROWS;

$func$ LANGUAGE 'sql';

CREATE OR REPLACE FUNCTION fill_permlink_cache () RETURNS VOID AS $func$
BEGIN

	INSERT INTO hive_permlink_data(permlink) SELECT tp.permlink FROM tmp_permlinks tp LEFT JOIN hive_permlink_data hpd ON tp.permlink=hpd.permlink WHERE id IS NULL;
	INSERT INTO tmp_legacy_permlinks SELECT hpd.permlink, id FROM tmp_permlinks tp LEFT JOIN hive_permlink_data hpd ON tp.permlink=hpd.permlink;

END
$func$ LANGUAGE 'plpgsql';

CREATE OR REPLACE FUNCTION prepare_accounts_cache () RETURNS VOID AS $func$

	CREATE TEMPORARY TABLE IF NOT EXISTS tmp_accounts( acc TEXT UNIQUE ) ON COMMIT DELETE ROWS;
	CREATE TEMPORARY TABLE IF NOT EXISTS tmp_legacy_accounts( acc TEXT UNIQUE, tid INTEGER ) ON COMMIT DELETE ROWS;

$func$ LANGUAGE 'sql';

CREATE OR REPLACE FUNCTION fill_accounts_cache () RETURNS VOID AS $func$
BEGIN

	INSERT INTO hive_accounts("name") SELECT ta.acc FROM tmp_accounts ta LEFT JOIN hive_accounts ha ON ta.acc=ha.name WHERE id IS NULL;
	INSERT INTO tmp_legacy_accounts SELECT ha.name, ha.id FROM tmp_accounts ta LEFT JOIN hive_accounts ha ON ta.acc=ha.name;

END
$func$ LANGUAGE 'plpgsql';
