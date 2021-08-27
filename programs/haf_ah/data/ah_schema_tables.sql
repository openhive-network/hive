CREATE TABLE IF NOT EXISTS public.accounts
(
  id SERIAL NOT NULL PRIMARY KEY,
  name VARCHAR(16) NOT NULL
)INHERITS( hive.account_history );

ALTER TABLE public.accounts ADD CONSTRAINT accounts_uniq UNIQUE ( name );

CREATE TABLE IF NOT EXISTS public.account_operations
(
  account_id INTEGER NOT NULL,        --- Identifier of account involved in given operation.
  account_op_seq_no INTEGER NOT NULL, --- Operation sequence number specific to given account. 
  operation_id BIGINT NOT NULL        --- Id of operation held in hive.opreations table.
)INHERITS( hive.account_history );

ALTER TABLE public.account_operations ADD CONSTRAINT account_operations_uniq UNIQUE ( account_id, account_op_seq_no );
CREATE INDEX IF NOT EXISTS account_operations_operation_id_idx ON account_operations (operation_id);

CREATE TABLE IF NOT EXISTS public.internal_account_operations
(
  operation_id BIGINT NOT NULL,
  name VARCHAR(16) NOT NULL
)INHERITS( hive.account_history );

CREATE INDEX IF NOT EXISTS internal_account_operations_ix1 ON public.internal_account_operations (operation_id);
CREATE INDEX IF NOT EXISTS internal_account_operations_ix2 ON public.internal_account_operations (name);

DROP FUNCTION IF EXISTS public.find_op_max;
CREATE FUNCTION public.find_op_max( in _ACCOUNT_ID INT )
RETURNS BIGINT
AS
$function$
declare __MAX_OP INT;
BEGIN

  SELECT ao.account_op_seq_no INTO __MAX_OP
  FROM public.account_operations ao
  WHERE ao.account_id = _ACCOUNT_ID ORDER BY ao.account_op_seq_no DESC LIMIT 1;

  IF __MAX_OP IS NULL THEN
    __MAX_OP = 1;
  ELSE
    __MAX_OP = __MAX_OP + 1;
  END IF;

  RETURN __MAX_OP;
END
$function$
language plpgsql VOLATILE;

DROP FUNCTION IF EXISTS public.fill_accounts_operations;
CREATE FUNCTION public.fill_accounts_operations( in _FIRST_BLOCK INT, in _LAST_BLOCK INT )
RETURNS VOID
AS
$function$
BEGIN

  ---------set internal data---------
  TRUNCATE internal_account_operations;
  INSERT INTO internal_account_operations(operation_id, name)
  SELECT ho.id, account_name
  FROM
  hive.operations ho, LATERAL get_impacted_accounts(body) account_name
  WHERE ho.block_num >= _FIRST_BLOCK AND ho.block_num <= _LAST_BLOCK;

  ---------add `accounts`---------
  INSERT INTO public.accounts( name )
    SELECT DISTINCT iao.name
    FROM public.internal_account_operations iao
    LEFT JOIN public.accounts pa ON iao.name = pa.name
    WHERE pa.name IS NULL;

  ---------add `account_operations`---------
  INSERT INTO public.account_operations(account_id, account_op_seq_no, operation_id)
    SELECT pa.id, public.find_op_max( pa.id ), iao.operation_id
    FROM public.internal_account_operations iao JOIN public.accounts pa ON iao.name = pa.name;
END
$function$
language plpgsql;
