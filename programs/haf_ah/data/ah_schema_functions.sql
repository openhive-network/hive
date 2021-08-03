DROP FUNCTION IF EXISTS public.get_ops_in_block;
CREATE FUNCTION public.get_ops_in_block( in _BLOCK_NUM INT, in _ONLY_VIRTUAL BOOLEAN )
RETURNS TABLE(
    _trx_id TEXT,
    _trx_in_block BIGINT,
    _op_in_trx BIGINT,
    _virtual_op BOOLEAN,
    _timestamp TEXT,
    _value TEXT,
    _operation_id INT
)
AS
$function$
BEGIN

  RETURN QUERY
    SELECT
      (
        CASE
        WHEN ht.trx_hash IS NULL THEN '0000000000000000000000000000000000000000'
        ELSE encode( ht.trx_hash, 'escape')
        END
      ) _trx_id,
      (
        CASE
        WHEN ht.trx_in_block IS NULL THEN 4294967295
        ELSE ht.trx_in_block
        END
      ) _trx_in_block,
      T.op_pos _op_in_trx,
      T.is_virtual _virtual_op,
      trim(both '"' from to_json(hb.created_at)::text) _timestamp,
      T.body _value,
      T.id::INT _operation_id
    FROM
      (
        --`abs` it's temporary, until position of operation is correctly saved
        SELECT
          ho.id, ho.block_num, ho.trx_in_block, abs(ho.op_pos::BIGINT) op_pos, ho.body, ho.op_type_id, hot.is_virtual
        FROM hive.account_history_operations_view ho
        JOIN hive.operation_types hot ON hot.id = ho.op_type_id
        WHERE ho.block_num = _BLOCK_NUM AND ( _ONLY_VIRTUAL = FALSE OR ( _ONLY_VIRTUAL = TRUE AND hot.is_virtual = TRUE ) )
      ) T
      JOIN hive.account_history_blocks_view hb ON hb.num = T.block_num
      LEFT JOIN hive.account_history_transactions_view ht ON T.block_num = ht.block_num AND T.trx_in_block = ht.trx_in_block;
END
$function$
language plpgsql STABLE;

DROP FUNCTION IF EXISTS public.get_transaction;
CREATE FUNCTION public.get_transaction( in _TRX_HASH BYTEA )
RETURNS TABLE(
    _ref_block_num INT,
    _ref_block_prefix BIGINT,
    _expiration TEXT,
    _block_num INT,
    _trx_in_block SMALLINT,
    _signature TEXT,
    _multisig_number SMALLINT
)
AS
$function$
DECLARE
  __multisig_number SMALLINT;
BEGIN

  SELECT count(*) INTO __multisig_number FROM hive.account_history_transactions_multisig_view htm WHERE htm.trx_hash = _TRX_HASH;

  RETURN QUERY
    SELECT
      ref_block_num _ref_block_num,
      ref_block_prefix _ref_block_prefix,
      '2016-06-20T19:34:09' _expiration,--lack of data
      ht.block_num _block_num,
      ht.trx_in_block _trx_in_block,
      encode(ht.signature, 'escape') _signature,
      __multisig_number
    FROM hive.account_history_transactions_view ht
    WHERE ht.trx_hash = _TRX_HASH;
END
$function$
language plpgsql STABLE;

DROP FUNCTION IF EXISTS public.get_multi_signatures_in_transaction;
CREATE FUNCTION public.get_multi_signatures_in_transaction( in _TRX_HASH BYTEA )
RETURNS TABLE(
    _signature TEXT
)
AS
$function$
BEGIN

  RETURN QUERY
    SELECT
      encode(htm.signature, 'escape') _signature
    FROM hive.account_history_transactions_multisig_view htm
    WHERE htm.trx_hash = _TRX_HASH;
END
$function$
language plpgsql STABLE;

DROP FUNCTION IF EXISTS public.get_ops_in_transaction;
CREATE FUNCTION public.get_ops_in_transaction( in _BLOCK_NUM INT, in _TRX_IN_BLOCK INT )
RETURNS TABLE(
    _value TEXT
)
AS
$function$
BEGIN
  RETURN QUERY
    SELECT
      ho.body _value
    FROM hive.account_history_operations_view ho
    JOIN hive.operation_types hot ON ho.op_type_id = hot.id
    WHERE ho.block_num = _BLOCK_NUM AND ho.trx_in_block = _TRX_IN_BLOCK AND hot.is_virtual = FALSE
    ORDER BY ho.id;
END
$function$
language plpgsql STABLE;

DROP FUNCTION IF EXISTS public.enum_virtual_ops;
CREATE FUNCTION public.enum_virtual_ops( in _FILTER INT[], in _BLOCK_RANGE_BEGIN INT, in _BLOCK_RANGE_END INT, _OPERATION_BEGIN BIGINT, in _LIMIT INT )
RETURNS TABLE(
    _trx_id TEXT,
    _block INT,
    _trx_in_block BIGINT,
    _op_in_trx BIGINT,
    _virtual_op BOOLEAN,
    _timestamp TEXT,
    _value TEXT,
    _operation_id INT
)
AS
$function$
DECLARE
  __filter_info INT;
BEGIN

   SELECT INTO __filter_info ( select array_length( _FILTER, 1 ) );

  RETURN QUERY
    SELECT
      (
        CASE
        WHEN ht.trx_hash IS NULL THEN '0000000000000000000000000000000000000000'
        ELSE encode( ht.trx_hash, 'escape')
        END
      ) _trx_id,
      T.block_num _block,
      (
        CASE
        WHEN ht.trx_in_block IS NULL THEN 4294967295
        ELSE ht.trx_in_block
        END
      ) _trx_in_block,
      T.op_pos _op_in_trx,
      TRUE _virtual_op,
      trim(both '"' from to_json(hb.created_at)::text) _timestamp,
      T.body _value,
      T.id::INT _operation_id
    FROM
      (
        --`abs` it's temporary, until position of operation is correctly saved
        SELECT
          ho.id, ho.block_num, ho.trx_in_block, abs(ho.op_pos::BIGINT) op_pos, ho.body, ho.op_type_id
          FROM hive.account_history_operations_view ho
          JOIN hive.operation_types hot ON hot.id = ho.op_type_id
          WHERE ho.block_num >= _BLOCK_RANGE_BEGIN AND ho.block_num < _BLOCK_RANGE_END
          AND hot.is_virtual = TRUE
          AND ( ( __filter_info IS NULL ) OR ( ho.op_type_id = ANY( _FILTER ) ) )
          AND ( _OPERATION_BEGIN = -1 OR ho.id >= _OPERATION_BEGIN )
          --There is `+1` because next block/operation is needed in order to do correct paging. This won't be put into result set.
          ORDER BY ho.id
          LIMIT _LIMIT + 1
      ) T
      JOIN hive.account_history_blocks_view hb ON hb.num = T.block_num
      LEFT JOIN hive.account_history_transactions_view ht ON T.block_num = ht.block_num AND T.trx_in_block = ht.trx_in_block;
END
$function$
language plpgsql STABLE;

DROP FUNCTION IF EXISTS public.ah_get_account_history;
CREATE FUNCTION public.ah_get_account_history( in _FILTER INT[], in _ACCOUNT VARCHAR, _START INT, _LIMIT INT )
RETURNS TABLE(
    _trx_id TEXT,
    _block INT,
    _trx_in_block BIGINT,
    _op_in_trx BIGINT,
    _virtual_op BOOLEAN,
    _timestamp TEXT,
    _value TEXT,
    _operation_id INT
)
AS
$function$
DECLARE
  __account_id INT;
  __filter_info INT;
BEGIN

  SELECT INTO __filter_info ( select array_length( _FILTER, 1 ) );

  SELECT INTO __account_id ( select id from public.accounts where name = _ACCOUNT );

  IF __filter_info IS NULL THEN
  RETURN QUERY
    SELECT
      (
        CASE
        WHEN ht.trx_hash IS NULL THEN '0000000000000000000000000000000000000000'
        ELSE encode( ht.trx_hash, 'escape')
        END
      ) _trx_id,
      ho.block_num _block,
      (
        CASE
        WHEN ht.trx_in_block IS NULL THEN 4294967295
        ELSE ht.trx_in_block
        END
      ) _trx_in_block,
      abs(ho.op_pos::BIGINT) AS _op_in_trx,
      hot.is_virtual _virtual_op,
      trim(both '"' from to_json(hb.created_at)::text) _timestamp,
      ho.body _value,
      T.seq_no AS _operation_id
      FROM
      (
        SELECT hao.operation_id as operation_id, hao.account_op_seq_no as seq_no
        FROM public.account_operations hao 
        WHERE hao.account_id = __account_id AND hao.account_op_seq_no <= _START
        ORDER BY seq_no DESC
        LIMIT _LIMIT
      ) T
    JOIN hive.operations ho ON T.operation_id = ho.id
    JOIN hive.blocks hb ON hb.num = ho.block_num
    JOIN hive.operation_types hot ON hot.id = ho.op_type_id
    LEFT JOIN hive.transactions ht ON ho.block_num = ht.block_num AND ho.trx_in_block = ht.trx_in_block
    LIMIT _LIMIT;
  ELSE
    RETURN QUERY
      SELECT
        (
          CASE
          WHEN ht.trx_hash IS NULL THEN '0000000000000000000000000000000000000000'
          ELSE encode( ht.trx_hash, 'escape')
          END
        ) _trx_id,
        T.block_num _block,
        (
          CASE
          WHEN ht.trx_in_block IS NULL THEN 4294967295
          ELSE ht.trx_in_block
          END
        ) _trx_in_block,
        T.op_pos _op_in_trx,
        hot.is_virtual _virtual_op,
        trim(both '"' from to_json(hb.created_at)::text) _timestamp,
        T.body _value,
        T.seq_no as _operation_id
      FROM
        (
          --`abs` it's temporary, until position of operation is correctly saved
          SELECT
            ho.id, ho.block_num, ho.trx_in_block, abs(ho.op_pos::BIGINT) op_pos, ho.body, ho.op_type_id, hao.account_op_seq_no as seq_no
            FROM hive.operations ho
            JOIN public.account_operations hao ON ho.id = hao.operation_id
            WHERE hao.account_id = __account_id AND hao.account_op_seq_no <= _START
            AND ( ho.op_type_id = ANY( _FILTER ) ) 
            ORDER BY seq_no DESC
            LIMIT _LIMIT
        ) T
        JOIN hive.blocks hb ON hb.num = T.block_num
        JOIN hive.operation_types hot ON hot.id = T.op_type_id
        LEFT JOIN hive.transactions ht ON T.block_num = ht.block_num AND T.trx_in_block = ht.trx_in_block
      LIMIT _LIMIT;

  END IF;

END
$function$
language plpgsql STABLE;

DROP VIEW IF EXISTS public.account_operation_count_info_view CASCADE;
CREATE OR REPLACE VIEW public.account_operation_count_info_view
AS
SELECT ha.id, ha.name, COALESCE( T.operation_count, 0 ) operation_count
FROM public.accounts ha
LEFT JOIN
(
SELECT ao.account_id account_id, COUNT(ao.account_op_seq_no) operation_count
FROM public.account_operations ao
GROUP BY ao.account_id
)T ON ha.id = T.account_id
;
