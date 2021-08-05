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
