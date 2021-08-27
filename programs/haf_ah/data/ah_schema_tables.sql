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

CREATE TABLE IF NOT EXISTS public.internal_data
(
  first_block INTEGER NOT NULL,
  last_block INTEGER NOT NULL
)INHERITS( hive.account_history );

DROP MATERIALIZED VIEW IF EXISTS public.account_operations_view;
CREATE MATERIALIZED VIEW public.account_operations_view AS
  SELECT ho.id, account_name
  FROM
  public.internal_data pid,
  hive.operations ho, LATERAL get_impacted_accounts(body) account_name
  WHERE ho.block_num >= pid.first_block AND ho.block_num <= pid.last_block;

CREATE INDEX IF NOT EXISTS account_operations_view_ix1 ON public.account_operations_view (id);
CREATE INDEX IF NOT EXISTS account_operations_view_ix2 ON public.account_operations_view (account_name);

DROP FUNCTION IF EXISTS public.find_op_max;
CREATE FUNCTION public.find_op_max( in _ACCOUNT_ID INT )
RETURNS BIGINT
AS
$function$
declare __MAX_OP INT;
BEGIN

	SELECT COALESCE( ao.account_op_seq_no, 0 ) INTO __MAX_OP
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

  ---------add `account_operations`---------
  INSERT INTO public.account_operations(account_id, account_op_seq_no, operation_id)
    SELECT pa.id, public.find_op_max( pa.id ), aov.id
    FROM public.account_operations_view aov JOIN public.accounts pa ON aov.account_name = pa.name;
END
$function$
language plpgsql;
