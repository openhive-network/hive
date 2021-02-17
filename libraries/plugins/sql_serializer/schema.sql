DROP SCHEMA public CASCADE;
CREATE SCHEMA public;
CREATE EXTENSION IF NOT EXISTS intarray;

CREATE TABLE IF NOT EXISTS hive_blocks (
  "num" integer NOT NULL,
  "hash" bytea NOT NULL,
  "prev" bytea NOT NULL,
  "created_at" timestamp without time zone NOT NULL
);

CREATE TABLE IF NOT EXISTS hive_transactions (
  "block_num" integer NOT NULL,
  "trx_in_block" smallint NOT NULL,
  "trx_hash" bytea NOT NULL,
  ref_block_num integer NOT NULL,
  ref_block_prefix bigint NOT NULL,
  signature bytea DEFAULT NULL
);

CREATE TABLE IF NOT EXISTS hive_transactions_multisig (
  "trx_hash" bytea NOT NULL,
  signature bytea NOT NULL
);

CREATE TABLE IF NOT EXISTS hive_operation_types (
  "id" smallint NOT NULL,
  "name" text NOT NULL,
  "is_virtual" boolean NOT NULL
);

CREATE TABLE IF NOT EXISTS hive_permlink_data (
  id INTEGER NOT NULL,
  permlink varchar(255) NOT NULL
);

CREATE TABLE IF NOT EXISTS hive_operations (
  id bigint not null,
  block_num integer NOT NULL,
  trx_in_block smallint NOT NULL,
  op_pos integer NOT NULL,
  op_type_id smallint NOT NULL,
  body text DEFAULT NULL
);

CREATE TABLE IF NOT EXISTS hive_accounts (
  id INTEGER NOT NULL,
  name VARCHAR(16) NOT NULL
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
SELECT ha.id, ha.name, COALESCE( T.operation_count, 0 ) operation_count
FROM hive_accounts ha
LEFT JOIN
(
SELECT ao.account_id account_id, COUNT(ao.account_op_seq_no) operation_count
FROM hive_account_operations ao
GROUP BY ao.account_id
)T ON ha.id = T.account_id
;

INSERT INTO hive_permlink_data VALUES(0, '');
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
; 

END
$function$
LANGUAGE plpgsql STABLE
;

DROP TYPE IF EXISTS hive_api_hivemind_blocks CASCADE;
CREATE TYPE hive_api_hivemind_blocks AS (
    num INTEGER,
    hash BYTEA,
    prev BYTEA,
    date TEXT,
    tx_number BIGINT,
    op_number BIGINT
    );


CREATE OR REPLACE FUNCTION enum_blocks4hivemind(in _first_block INT, in _last_block INT)
RETURNS SETOF hive_api_hivemind_blocks
AS
$function$
BEGIN
RETURN QUERY
SELECT
    hb.num
     , hb.hash
     , hb.prev as prev
     , to_char( created_at,  'YYYY-MM-DDThh24:MI:SS' ) as date
        , ( SELECT COUNT(*) tx_number  FROM hive_transactions ht WHERE ht.block_num = hb.num ) as tx_number
        , ( SELECT COUNT(*) op_number  FROM hive_operations ho WHERE ho.block_num = hb.num AND ( ho.op_type_id < 48 OR ho.op_type_id in (49, 51, 59, 70, 71) ) ) as op_number
FROM hive_blocks hb
WHERE hb.num >= _first_block AND hb.num < _last_block
ORDER by hb.num
;
END
$function$
LANGUAGE plpgsql STABLE
;

DROP FUNCTION IF EXISTS index_updater;
CREATE OR REPLACE FUNCTION index_updater(in _mode BOOLEAN)
RETURNS VOID
AS
$function$
BEGIN

----------------dropping all indexes----------------
  DROP INDEX IF EXISTS hive_account_operations_account_op_seq_no_id_idx;
  DROP INDEX IF EXISTS hive_accounts_name_idx;
  DROP INDEX IF EXISTS hive_operations_block_num_type_trx_in_block_idx;
  DROP INDEX IF EXISTS hive_transactions_block_num_trx_in_block_idx;

  IF _mode = True THEN
----------------creating all indexes----------------
    CREATE INDEX IF NOT EXISTS hive_account_operations_account_op_seq_no_id_idx ON hive_account_operations(account_id, account_op_seq_no, operation_id);
    CREATE INDEX IF NOT EXISTS hive_accounts_name_idx ON hive_accounts(name);
    CREATE INDEX IF NOT EXISTS hive_operations_block_num_type_trx_in_block_idx ON hive_operations (block_num, op_type_id, trx_in_block);
    CREATE INDEX IF NOT EXISTS hive_transactions_block_num_trx_in_block_idx ON hive_transactions (block_num, trx_in_block);

  END IF;

END
$function$
LANGUAGE plpgsql VOLATILE
;

DROP FUNCTION IF EXISTS constraint_primary_unique_updater;
CREATE OR REPLACE FUNCTION constraint_primary_unique_updater(in _mode BOOLEAN)
RETURNS VOID
AS
$function$
BEGIN

----------------dropping all primary keys----------------
ALTER TABLE hive_accounts DROP CONSTRAINT IF EXISTS hive_accounts_pkey;
ALTER TABLE hive_blocks DROP CONSTRAINT IF EXISTS hive_blocks_pkey;
ALTER TABLE hive_operation_types DROP CONSTRAINT IF EXISTS hive_operation_types_pkey;
ALTER TABLE hive_operations DROP CONSTRAINT IF EXISTS hive_operations_pkey;
ALTER TABLE hive_permlink_data DROP CONSTRAINT IF EXISTS hive_permlink_data_pkey;
ALTER TABLE hive_transactions DROP CONSTRAINT IF EXISTS hive_transactions_pkey;

----------------dropping others constraints----------------
ALTER TABLE hive_accounts DROP CONSTRAINT IF EXISTS hive_accounts_uniq;
ALTER TABLE hive_blocks DROP CONSTRAINT IF EXISTS hive_blocks_uniq;
ALTER TABLE hive_operation_types DROP CONSTRAINT IF EXISTS hive_operation_types_uniq;
ALTER TABLE hive_permlink_data DROP CONSTRAINT IF EXISTS hive_permlink_data_uniq;


IF _mode = True THEN
----------------creating all primary keys----------------
ALTER TABLE hive_accounts ADD CONSTRAINT hive_accounts_pkey PRIMARY KEY ( id );
ALTER TABLE hive_blocks ADD CONSTRAINT hive_blocks_pkey PRIMARY KEY ( num );
ALTER TABLE hive_operation_types ADD CONSTRAINT hive_operation_types_pkey PRIMARY KEY ( id );
ALTER TABLE hive_operations ADD CONSTRAINT hive_operations_pkey PRIMARY KEY ( id );
ALTER TABLE hive_permlink_data ADD CONSTRAINT hive_permlink_data_pkey PRIMARY KEY ( id );
ALTER TABLE hive_transactions ADD CONSTRAINT hive_transactions_pkey PRIMARY KEY ( trx_hash );

----------------creating others constraints----------------
ALTER TABLE hive_accounts ADD CONSTRAINT hive_accounts_uniq UNIQUE ( name );
ALTER TABLE hive_blocks ADD CONSTRAINT hive_blocks_uniq UNIQUE ( hash );
ALTER TABLE hive_operation_types ADD CONSTRAINT hive_operation_types_uniq UNIQUE ( name );
ALTER TABLE hive_permlink_data ADD CONSTRAINT hive_permlink_data_uniq UNIQUE ( permlink );

END IF;

END
$function$
LANGUAGE plpgsql VOLATILE
;

DROP FUNCTION IF EXISTS constraint_foreign_keys_updater;
CREATE OR REPLACE FUNCTION constraint_foreign_keys_updater(in _mode BOOLEAN)
RETURNS VOID
AS
$function$
BEGIN

----------------dropping all foreign keys----------------
ALTER TABLE hive_account_operations DROP CONSTRAINT IF EXISTS hive_account_operations_fk_1;
ALTER TABLE hive_account_operations DROP CONSTRAINT IF EXISTS hive_account_operations_fk_2;

ALTER TABLE hive_operations DROP CONSTRAINT IF EXISTS hive_operations_fk_1;
ALTER TABLE hive_operations DROP CONSTRAINT IF EXISTS hive_operations_fk_2;

ALTER TABLE hive_transactions DROP CONSTRAINT IF EXISTS hive_transactions_fk_1;
ALTER TABLE hive_transactions_multisig DROP CONSTRAINT IF EXISTS hive_transactions_multisig_fk_1;

IF _mode = True THEN
----------------creating all foreign keys----------------
ALTER TABLE hive_account_operations ADD CONSTRAINT hive_account_operations_fk_1 FOREIGN KEY (account_id) REFERENCES hive_accounts (id);
ALTER TABLE hive_account_operations ADD CONSTRAINT hive_account_operations_fk_2 FOREIGN KEY (operation_id) REFERENCES hive_operations (id);

ALTER TABLE hive_operations ADD CONSTRAINT hive_operations_fk_1 FOREIGN KEY (block_num) REFERENCES hive_blocks (num);
ALTER TABLE hive_operations ADD CONSTRAINT hive_operations_fk_2 FOREIGN KEY (op_type_id) REFERENCES hive_operation_types (id);

ALTER TABLE hive_transactions ADD CONSTRAINT hive_transactions_fk_1 FOREIGN KEY (block_num) REFERENCES hive_blocks (num);

ALTER TABLE hive_transactions_multisig ADD CONSTRAINT hive_transactions_multisig_fk_1 FOREIGN KEY (trx_hash) REFERENCES hive_transactions (trx_hash);

END IF;

END
$function$
LANGUAGE plpgsql VOLATILE
;
