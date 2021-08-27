CREATE TABLE IF NOT EXISTS public.accounts
(
  id SERIAL NOT NULL PRIMARY KEY,
  name VARCHAR(16) NOT NULL,
  account_op_seq_total_no INTEGER DEFAULT 0
)INHERITS( hive.%s );

ALTER TABLE accounts ADD CONSTRAINT accounts_uniq UNIQUE ( name );

CREATE TABLE IF NOT EXISTS public.account_operations
(
  account_id INTEGER NOT NULL,        --- Identifier of account involved in given operation.
  account_op_seq_no INTEGER NOT NULL, --- Operation sequence number specific to given account. 
  operation_id BIGINT NOT NULL        --- Id of operation held in hive.opreations table.
)INHERITS( hive.%s );

CREATE INDEX IF NOT EXISTS account_operations_account_op_seq_no_id_idx ON account_operations(account_id, account_op_seq_no, operation_id);
CREATE INDEX IF NOT EXISTS account_operations_operation_id_idx ON account_operations (operation_id);

CREATE TABLE IF NOT EXISTS public.internal_account_operations
(
  account_id INTEGER NOT NULL,
  account_op_seq_no INTEGER DEFAULT NULL,
  operation_id BIGINT NOT NULL
)INHERITS( hive.%s );

CREATE INDEX IF NOT EXISTS internal_account_operations_account_op_seq_no_id_idx ON internal_account_operations(account_id, account_op_seq_no, operation_id);
CREATE INDEX IF NOT EXISTS internal_account_operations_operation_id_idx ON internal_account_operations (operation_id);

CREATE TABLE IF NOT EXISTS public.internal_data
(
  first_block INTEGER NOT NULL,
  last_block INTEGER NOT NULL
)INHERITS( hive.%s );

DROP MATERIALIZED VIEW IF EXISTS public.account_operations_view;
CREATE MATERIALIZED VIEW public.account_operations_view AS
  SELECT ho.id, account_name
  FROM
  public.internal_data pid,
  hive.operations ho, LATERAL get_impacted_accounts(body) account_name
  WHERE ho.block_num >= pid.first_block AND ho.block_num <= pid.last_block;

CREATE INDEX IF NOT EXISTS account_operations_view_ix1 ON public.account_operations_view (id);
CREATE INDEX IF NOT EXISTS account_operations_view_ix2 ON public.account_operations_view (account_name);

DROP FUNCTION IF EXISTS public.fill_accounts_operations;
CREATE FUNCTION public.fill_accounts_operations( in _FIRST_BLOCK INT, in _LAST_BLOCK INT )
RETURNS VOID
AS
$function$
declare __NR_EMPTY_RECORDS INT;
declare __INTERNAL_CHECKER INT;
BEGIN

  ---------set internal data---------
  SELECT COUNT(*) INTO __INTERNAL_CHECKER FROM public.internal_data;
  IF __INTERNAL_CHECKER = 0 THEN
  	INSERT INTO public.internal_data( first_block, last_block ) VALUES( _FIRST_BLOCK, _LAST_BLOCK );
  ELSE
    UPDATE public.internal_data SET first_block = _FIRST_BLOCK, last_block = _LAST_BLOCK;
  END IF;

  REFRESH MATERIALIZED VIEW public.account_operations_view;

  ---------add `accounts`---------
  INSERT INTO public.accounts( name )
    SELECT DISTINCT aov.account_name
    FROM public.account_operations_view aov
    LEFT JOIN public.accounts pa ON aov.account_name = pa.name
    WHERE pa.name IS NULL;

  ---------prepare auxiliary table---------
  DELETE FROM public.internal_account_operations;
  INSERT INTO public.internal_account_operations(account_id, operation_id)
    SELECT pa.id, aov.id
    FROM public.account_operations_view aov JOIN public.accounts pa ON aov.account_name = pa.name;

  ---------check condition of the end---------
  SELECT COUNT(*) INTO __NR_EMPTY_RECORDS
  FROM public.internal_account_operations piao
  WHERE piao.account_op_seq_no IS NULL;

  WHILE __NR_EMPTY_RECORDS > 0 LOOP

    ---------generate sequences---------
    UPDATE public.internal_account_operations updated
    SET account_op_seq_no = pa.account_op_seq_total_no + 1
    FROM
    (
      SELECT min(piao.operation_id) min_op_id, account_id
      FROM public.internal_account_operations piao
      WHERE piao.account_op_seq_no IS NULL
      GROUP BY account_id
    )T_MIN
    JOIN
    public.accounts pa
    ON T_MIN.account_id = pa.id
    WHERE updated.account_op_seq_no IS NULL
    AND updated.operation_id = T_MIN.min_op_id AND updated.account_id = T_MIN.account_id;

    ---------update counters---------
    UPDATE public.accounts updated
    SET account_op_seq_total_no = T.max_account_op_seq_no
    FROM
    (
      SELECT piao.account_id, max(piao.account_op_seq_no) max_account_op_seq_no
      FROM public.internal_account_operations piao
      GROUP BY piao.account_id
    )T
    WHERE updated.id = T.account_id;

    ---------check condition of the end---------
    SELECT COUNT(*) INTO __NR_EMPTY_RECORDS
    FROM public.internal_account_operations piao
    WHERE piao.account_op_seq_no IS NULL;

  END LOOP;

  ---------add `account_operations`---------
  INSERT INTO account_operations(account_id, account_op_seq_no, operation_id)
    SELECT account_id, account_op_seq_no, operation_id
    FROM internal_account_operations;

END
$function$
language plpgsql;
