DROP SCHEMA public CASCADE;
CREATE SCHEMA public;
CREATE EXTENSION IF NOT EXISTS intarray;

CREATE TABLE IF NOT EXISTS hive_blocks (
  "num" integer NOT NULL,
  "hash" bytea NOT NULL,
  "prev" bytea NOT NULL,
  "created_at" timestamp without time zone NOT NULL
);
ALTER TABLE hive_blocks ADD CONSTRAINT hive_blocks_pkey PRIMARY KEY ( num );
ALTER TABLE hive_blocks ADD CONSTRAINT hive_blocks_uniq UNIQUE ( hash );

CREATE TABLE IF NOT EXISTS hive_transactions (
  "block_num" integer NOT NULL,
  "trx_in_block" smallint NOT NULL,
  "trx_hash" bytea NOT NULL,
  ref_block_num integer NOT NULL,
  ref_block_prefix bigint NOT NULL,
  expiration timestamp without time zone NOT NULL,
  signature bytea DEFAULT NULL
);
ALTER TABLE hive_transactions ADD CONSTRAINT hive_transactions_pkey PRIMARY KEY ( trx_hash );
CREATE INDEX IF NOT EXISTS hive_transactions_block_num_trx_in_block_idx ON hive_transactions ( block_num, trx_in_block );

CREATE TABLE IF NOT EXISTS hive_transactions_multisig (
  "trx_hash" bytea NOT NULL,
  signature bytea NOT NULL
);
ALTER TABLE hive_transactions_multisig ADD CONSTRAINT hive_transactions_multisig_pkey PRIMARY KEY ( trx_hash, signature );

CREATE TABLE IF NOT EXISTS hive_operation_types (
  "id" smallint NOT NULL,
  "name" text NOT NULL,
  "is_virtual" boolean NOT NULL,
  CONSTRAINT hive_operation_types_pkey PRIMARY KEY (id),
  CONSTRAINT hive_operation_types_uniq UNIQUE (name)
);

CREATE TABLE IF NOT EXISTS hive_operations (
  id bigint not null,
  block_num integer NOT NULL,
  trx_in_block smallint NOT NULL,
  op_pos integer NOT NULL,
  op_type_id smallint NOT NULL,
  body text DEFAULT NULL
);
ALTER TABLE hive_operations ADD CONSTRAINT hive_operations_pkey PRIMARY KEY ( id );
CREATE INDEX IF NOT EXISTS hive_operations_block_num_type_trx_in_block_idx ON hive_operations ( block_num, op_type_id, trx_in_block );


ALTER TABLE hive_operations ADD CONSTRAINT hive_operations_fk_1 FOREIGN KEY (block_num) REFERENCES hive_blocks (num);
ALTER TABLE hive_operations ADD CONSTRAINT hive_operations_fk_2 FOREIGN KEY (op_type_id) REFERENCES hive_operation_types (id);
ALTER TABLE hive_transactions ADD CONSTRAINT hive_transactions_fk_1 FOREIGN KEY (block_num) REFERENCES hive_blocks (num);
ALTER TABLE hive_transactions_multisig ADD CONSTRAINT hive_transactions_multisig_fk_1 FOREIGN KEY (trx_hash) REFERENCES hive_transactions (trx_hash);


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

DROP TABLE IF EXISTS hive_indexes_constraints;
CREATE TABLE IF NOT EXISTS hive_indexes_constraints (
  "table_name" text NOT NULL,
  "index_constraint_name" text NOT NULL,
  "command" text NOT NULL,
  "is_constraint" boolean NOT NULL,
  "is_index" boolean NOT NULL,
  "is_foreign_key" boolean NOT NULL
);
ALTER TABLE hive_indexes_constraints ADD CONSTRAINT hive_indexes_constraints_table_name_idx UNIQUE ( table_name, index_constraint_name );

DROP FUNCTION IF EXISTS save_and_drop_indexes_constraints;
CREATE OR REPLACE FUNCTION save_and_drop_indexes_constraints( in _table_name TEXT )
RETURNS VOID
AS
$function$
DECLARE
  __command TEXT;
  __cursor REFCURSOR;
BEGIN

  INSERT INTO hive_indexes_constraints( table_name, index_constraint_name, command, is_constraint, is_index, is_foreign_key )
  SELECT
    T.table_name,
    T.constraint_name,
    (
      CASE
        WHEN T.is_primary = TRUE THEN 'ALTER TABLE ' || T.table_name || ' ADD CONSTRAINT ' || T.constraint_name || ' PRIMARY KEY ( ' || array_to_string(array_agg( T.column_name::TEXT ), ', ') || ' ) '
        WHEN (T.is_unique = TRUE AND T.is_primary = FALSE ) THEN 'ALTER TABLE ' || T.table_name || ' ADD CONSTRAINT ' || T.constraint_name || ' UNIQUE ( ' || array_to_string(array_agg( T.column_name::TEXT ), ', ') || ' ) '
        WHEN (T.is_unique = FALSE AND T.is_primary = FALSE ) THEN 'CREATE INDEX IF NOT EXISTS ' || T.constraint_name || ' ON ' || T.table_name || ' ( ' || array_to_string(array_agg( T.column_name::TEXT ), ', ') || ' ) '
      END
    ),
    (T.is_unique = TRUE OR T.is_primary = TRUE ) is_constraint,
    (T.is_unique = FALSE AND T.is_primary = FALSE ) is_index,
    FALSE is_foreign_key
  FROM
  (
    SELECT
        t.relname table_name,
        i.relname constraint_name,
        a.attname column_name,
        ix.indisunique is_unique,
        ix.indisprimary is_primary,
        a.attnum
    FROM
        pg_class t,
        pg_class i,
        pg_index ix,
        pg_attribute a
    WHERE
        t.oid = ix.indrelid
        AND i.oid = ix.indexrelid
        AND a.attrelid = t.oid
        AND a.attnum = ANY(ix.indkey)
        AND t.relkind = 'r'
        AND t.relname = _table_name
    ORDER BY t.relname, i.relname, a.attnum
  )T
  GROUP BY T.table_name, T.constraint_name, T.is_unique, T.is_primary
  ON CONFLICT DO NOTHING;

  --dropping indexes
  OPEN __cursor FOR ( SELECT ('DROP INDEX IF EXISTS '::TEXT || index_constraint_name || ';') FROM hive_indexes_constraints WHERE table_name = _table_name AND is_index = TRUE );
  LOOP
    FETCH __cursor INTO __command;
    EXIT WHEN NOT FOUND;
    EXECUTE __command;
  END LOOP;
  CLOSE __cursor;

  --dropping primary keys/unique contraints
  OPEN __cursor FOR ( SELECT ('ALTER TABLE '::TEXT || _table_name || ' DROP CONSTRAINT IF EXISTS ' || index_constraint_name || ';') FROM hive_indexes_constraints WHERE table_name = _table_name AND is_constraint = TRUE );
  LOOP
    FETCH __cursor INTO __command;
    EXIT WHEN NOT FOUND;
    EXECUTE __command;
  END LOOP;
  CLOSE __cursor;

END
$function$
LANGUAGE plpgsql VOLATILE
;

DROP FUNCTION IF EXISTS save_and_drop_indexes_foreign_keys;
CREATE OR REPLACE FUNCTION save_and_drop_indexes_foreign_keys( in _table_name TEXT )
RETURNS VOID
AS
$function$
DECLARE
  __command TEXT;
  __cursor REFCURSOR;
BEGIN

  INSERT INTO hive_indexes_constraints( table_name, index_constraint_name, command, is_constraint, is_index, is_foreign_key )
  SELECT
    tc.table_name,
    tc.constraint_name,
    'ALTER TABLE ' || tc.table_name || ' ADD CONSTRAINT ' || tc.constraint_name || ' FOREIGN KEY ( ' || kcu.column_name || ' ) REFERENCES ' || ccu.table_name || ' ( ' || ccu.column_name || ' ) ',
    FALSE is_constraint,
    FALSE is_index,
    TRUE is_foreign_key
  FROM 
      information_schema.table_constraints AS tc 
      JOIN information_schema.key_column_usage AS kcu ON tc.constraint_name = kcu.constraint_name AND tc.table_schema = kcu.table_schema
      JOIN information_schema.constraint_column_usage AS ccu ON ccu.constraint_name = tc.constraint_name AND ccu.table_schema = tc.table_schema
  WHERE tc.constraint_type = 'FOREIGN KEY' AND tc.table_name = _table_name
  ON CONFLICT DO NOTHING;

  OPEN __cursor FOR ( SELECT ('ALTER TABLE '::TEXT || _table_name || ' DROP CONSTRAINT IF EXISTS ' || index_constraint_name || ';') FROM hive_indexes_constraints WHERE table_name = _table_name AND is_foreign_key = TRUE );

  LOOP
    FETCH __cursor INTO __command;
    EXIT WHEN NOT FOUND;
    EXECUTE __command;
  END LOOP;

  CLOSE __cursor;

END
$function$
LANGUAGE plpgsql VOLATILE
;

DROP FUNCTION IF EXISTS restore_indexes_constraints;
CREATE OR REPLACE FUNCTION restore_indexes_constraints( in _table_name TEXT )
RETURNS VOID
AS
$function$
DECLARE
  __command TEXT;
  __cursor REFCURSOR;
BEGIN

  --restoring indexes, primary keys, unique contraints
  OPEN __cursor FOR ( SELECT command FROM hive_indexes_constraints WHERE table_name = _table_name AND is_foreign_key = FALSE );
  LOOP
    FETCH __cursor INTO __command;
    EXIT WHEN NOT FOUND;
    EXECUTE __command;
  END LOOP;
  CLOSE __cursor;

  DELETE FROM hive_indexes_constraints
  WHERE table_name = _table_name AND is_foreign_key = FALSE;

END
$function$
LANGUAGE plpgsql VOLATILE
;

DROP FUNCTION IF EXISTS restore_foreign_keys;
CREATE OR REPLACE FUNCTION restore_foreign_keys( in _table_name TEXT )
RETURNS VOID
AS
$function$
DECLARE
  __command TEXT;
  __cursor REFCURSOR;
BEGIN

  --restoring indexes, primary keys, unique contraints
  OPEN __cursor FOR ( SELECT command FROM hive_indexes_constraints WHERE table_name = _table_name AND is_foreign_key = TRUE );
  LOOP
    FETCH __cursor INTO __command;
    EXIT WHEN NOT FOUND;
    EXECUTE __command;
  END LOOP;
  CLOSE __cursor;

  DELETE FROM hive_indexes_constraints
  WHERE table_name = _table_name AND is_foreign_key = TRUE;

END
$function$
LANGUAGE plpgsql VOLATILE
;
